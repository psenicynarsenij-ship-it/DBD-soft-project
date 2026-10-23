#include "esp/runtime_provider.hpp"
#include <cmath>
#include <stdexcept>

namespace esp {
const char* SourceStateName(SourceState s) {
    switch(s) {
    case SourceState::Starting:return "STARTING";case SourceState::Live:return "LIVE";
    case SourceState::Reconnecting:return "RECONNECTING / STALE";case SourceState::Invalid:return "INVALID DATA";
    case SourceState::Ended:return "REPLAY ENDED";case SourceState::Stopped:return "STOPPED";
    }
    return "UNKNOWN";
}
RuntimeDataProvider::RuntimeDataProvider(std::unique_ptr<IFrameSource> source,RuntimeOptions options,std::filesystem::path recordingPath,ReadContext initialContext)
    :options_(std::move(options)),recordingPath_(std::move(recordingPath)),source_(std::move(source)) {
    ValidateRuntimeOptions(options_);if(!source_) throw std::invalid_argument("null frame source");
    if(options_.record&&recordingPath_.empty()) throw std::invalid_argument("recording path is required");
    SetContext(initialContext.aspect,initialContext.camera);
    auto initial=std::make_shared<RuntimeSnapshot>();initial->diagnostics.kind=options_.kind;initial->diagnostics.dbd=DescribeDbdConnection(options_.kind);
    latest_.store(initial);worker_=std::thread([this]{Run();});
}
RuntimeDataProvider::~RuntimeDataProvider() {Stop();}
void RuntimeDataProvider::SetContext(float aspect,const CameraSettings& camera) {
    if(!std::isfinite(aspect)||aspect<=0||!std::isfinite(camera.verticalFovDegrees)||
       camera.verticalFovDegrees<CameraSettings::MinFovDegrees||camera.verticalFovDegrees>CameraSettings::MaxFovDegrees)
        throw std::invalid_argument("invalid render context");
    std::lock_guard lock(mutex_);context_.aspect=aspect;context_.camera=camera;
}
std::shared_ptr<const RuntimeSnapshot> RuntimeDataProvider::Latest() const {
    const auto snapshot=latest_.load(std::memory_order_acquire);
    const auto now=std::chrono::steady_clock::now();
    if(snapshot->validUntil&&now>=*snapshot->validUntil) {
        // A slow/stalled producer cannot leave stale objects visible on the render thread.
        static const auto empty=std::make_shared<const Frame>();
        auto expired=std::make_shared<RuntimeSnapshot>(*snapshot);expired->frame=empty;
        expired->diagnostics.state=SourceState::Reconnecting;expired->diagnostics.fallback=false;
        expired->diagnostics.dbd=DescribeDbdConnection(expired->diagnostics.kind);
        expired->diagnostics.message="Snapshot expired; waiting for source worker";
        expired->diagnostics.sourceFps=0;expired->validUntil.reset();return expired;
    }
    return snapshot;
}
Frame RuntimeDataProvider::Sample(float aspect,const CameraSettings& camera) {SetContext(aspect,camera);return *Latest()->frame;}
void RuntimeDataProvider::Stop() {
    std::lock_guard stopping(stopMutex_);
    {std::lock_guard lock(mutex_);stop_=true;}wake_.notify_all();
    if(worker_.joinable()) worker_.join();
}
void RuntimeDataProvider::Run() {
    using Clock=std::chrono::steady_clock;
    const auto empty=std::make_shared<const Frame>();
    std::shared_ptr<const Frame> current;
    std::optional<Clock::time_point> received,lastFresh;
    DemoFrameSource fallback;
    RuntimeDiagnostics d;d.kind=options_.kind;
    std::unique_ptr<FrameRecorder> recorder;bool recordFailed=false;
    for(;;) {
        ReadContext context;
        {std::lock_guard lock(mutex_);if(stop_) break;context=context_;}
        context.now=Clock::now();const auto started=context.now;
        SourceRead read;
        try {
            read=source_->Poll(context);
            if(read.state==ReadState::Ready) {
                if(!read.frame) throw std::runtime_error("source returned Ready without a frame");
                ValidateFrame(*read.frame);
            }
        } catch(const std::exception& e) {read={ReadState::Invalid,{},e.what()};}
        const auto now=Clock::now();d.acquisitionMs=std::chrono::duration<double,std::milli>(now-started).count();
        d.fallback=false;d.message=read.message.substr(0,512);
        bool fresh=false;
        if(read.state==ReadState::Ready) {
            read.frame->source=SourceKindName(options_.kind);
            current=std::make_shared<const Frame>(std::move(*read.frame));received=now;
            if(lastFresh) {const double dt=std::chrono::duration<double>(now-*lastFresh).count();if(dt>0) d.sourceFps=d.sourceFps==0?1/dt:0.85*d.sourceFps+0.15/dt;}
            lastFresh=now;++d.acceptedFrames;d.state=SourceState::Live;fresh=true;
        } else if(read.state==ReadState::Invalid) {
            current.reset();received.reset();lastFresh.reset();d.sourceFps=0;++d.invalidFrames;d.state=SourceState::Invalid;
        } else if(read.state==ReadState::Ended) {
            d.state=SourceState::Ended;d.sourceFps=0;
        } else if(read.state==ReadState::Scheduled) {
            d.state=current?SourceState::Live:SourceState::Starting;
        } else {
            const bool stale=!received||std::chrono::duration_cast<std::chrono::milliseconds>(now-*received).count()>=options_.staleMs;
            d.state=read.state==ReadState::Disconnected||stale?SourceState::Reconnecting:SourceState::Live;
            if(stale) {current.reset();lastFresh.reset();d.sourceFps=0;}
        }
        d.measuredAt=now;
        d.ageMs=received?std::chrono::duration<double,std::milli>(now-*received).count():0;
        auto displayed=current?current:empty;
        if(!current&&options_.fallbackDemo&&options_.kind!=SourceKind::Demo) {
            auto sample=fallback.Poll(context);
            sample.frame->source="DEMO FALLBACK - source unavailable";
            displayed=std::make_shared<const Frame>(std::move(*sample.frame));d.fallback=true;fresh=true;
        }
        if(options_.record&&!recordFailed&&fresh) {
            try {
                if(!recorder) recorder=std::make_unique<FrameRecorder>(recordingPath_);
                recorder->Write(*displayed,Clock::now());d.recording="RECORDING";
            } catch(const std::exception& e) {d.recording=std::string("RECORD ERROR: ")+e.what();recordFailed=true;recorder.reset();}
        } else if(!options_.record) d.recording="OFF";
        d.dbd=DescribeDbdConnection(options_.kind,d.fallback);
        auto snapshot=std::make_shared<RuntimeSnapshot>();snapshot->frame=std::move(displayed);snapshot->diagnostics=d;
        if(d.fallback) snapshot->validUntil=now+std::chrono::milliseconds(options_.staleMs);
        else if(current&&received&&read.state!=ReadState::Ended&&read.state!=ReadState::Scheduled)
            snapshot->validUntil=*received+std::chrono::milliseconds(options_.staleMs);
        latest_.store(std::move(snapshot),std::memory_order_release);
        std::unique_lock lock(mutex_);
        wake_.wait_until(lock,started+std::chrono::milliseconds(options_.pollMs),[&]{return stop_;});
        if(stop_) break;
    }
    auto stopped=std::make_shared<RuntimeSnapshot>();stopped->diagnostics=d;stopped->diagnostics.state=SourceState::Stopped;
    stopped->diagnostics.recording="OFF";stopped->diagnostics.fallback=false;stopped->diagnostics.dbd=DescribeDbdConnection(options_.kind);latest_.store(std::move(stopped),std::memory_order_release);
}
}
