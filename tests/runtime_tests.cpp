#include "esp/runtime_provider.hpp"
#include <atomic>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

namespace {
using namespace esp;
using Clock=std::chrono::steady_clock;
void Check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
template<class F> void Reject(F fn,const char* message) {bool rejected=false;try {fn();}catch(const std::exception&) {rejected=true;}Check(rejected,message);}
struct Temp {
    std::filesystem::path path=std::filesystem::temp_directory_path()/("esp-runtime-"+std::to_string(Clock::now().time_since_epoch().count()));
    Temp() {std::filesystem::create_directory(path);}
    ~Temp() {std::error_code ec;std::filesystem::remove_all(path,ec);}
};
Frame Fixture(std::uint64_t seq=1,std::size_t count=11) {
    DemoProvider demo;auto f=demo.SampleAt(16.0f/9.0f,CameraSettings{90},0);
    if(count<f.entities.size()) f.entities.resize(count);
    while(f.entities.size()<count) {auto e=f.entities.front();e.id=f.entities.size()+1;f.entities.push_back(std::move(e));}
    f.sequence=seq;f.timestampUs=seq;return f;
}
void Write(const std::filesystem::path& path,const std::string& text) {AtomicWriteText(path,text);}
template<class Predicate> std::shared_ptr<const RuntimeSnapshot> Wait(RuntimeDataProvider& provider,Predicate predicate) {
    const auto deadline=Clock::now()+std::chrono::seconds(4);
    while(Clock::now()<deadline) {
        const auto s=provider.Latest();if(predicate(*s)) return s;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("timed out waiting for runtime state");
}
RuntimeOptions FileOptions() {RuntimeOptions o;o.kind=SourceKind::JsonFile;o.pollMs=5;o.staleMs=100;return o;}
void Serialization() {
    auto f=Fixture(std::numeric_limits<std::uint64_t>::max());f.entities.front().id=std::numeric_limits<std::uint64_t>::max();
    f.entities.front().name="Survivor \xD0\x90";
    const auto text=SerializeFrame(f);const auto decoded=DeserializeFrame(text);
    Check(SerializeFrame(decoded)==text,"all frame fields round trip including uint64 and UTF-8");
    Check(decoded.entities.at(4).lookDirection.has_value()&&!decoded.entities.at(0).lookDirection,"optional directions round trip");
    Reject([&]{DeserializeFrame(text+"garbage");},"trailing JSON rejected");
    auto empty=f;empty.entities.clear();Check(DeserializeFrame(SerializeFrame(empty)).entities.empty(),"empty authoritative snapshot accepted");
}
void Validation() {
    auto bad=[](const std::function<void(Frame&)>& mutate) {auto f=Fixture();mutate(f);Reject([&]{ValidateFrame(f);},"invalid frame accepted");};
    bad([](Frame& f){f.camera.position.x=std::numeric_limits<float>::quiet_NaN();});
    bad([](Frame& f){f.camera.viewProjection.m[0]=std::numeric_limits<float>::infinity();});
    bad([](Frame& f){f.camera.viewProjection={};});
    bad([](Frame& f){for(int i=0;i<4;++i) f.camera.viewProjection.m[4+i]=f.camera.viewProjection.m[i];});
    bad([](Frame& f){f.camera.viewProjection.m[0]=1e7f;});
    bad([](Frame& f){f.entities[0].id=0;});
    bad([](Frame& f){f.entities[1].id=f.entities[0].id;});
    bad([](Frame& f){f.entities[0].position.x=2e6f;});
    bad([](Frame& f){f.entities[0].top.z=std::numeric_limits<float>::infinity();});
    bad([](Frame& f){f.entities[0].lookDirection=Vec3{};});
    bad([](Frame& f){f.entities[0].lookDirection=Vec3{0,0,2};});
    bad([](Frame& f){f.entities[0].kind=static_cast<EntityKind>(99);});
    bad([](Frame& f){f.entities[0].name=std::string(257,'a');});
    bad([](Frame& f){f.entities[0].name="\xff";});
    bad([](Frame& f){f.source="label\nspoof";});
    Reject([]{ValidateFrame(Fixture(1,MaxFrameEntities+1));},"object limit enforced");
    Reject([]{DeserializeFrame(std::string(MaxFrameBytes+1,' '));},"JSON size bounded before parse");
    Reject([]{DeserializeFrame(std::string(64,'[')+"0"+std::string(64,']'));},"JSON nesting bounded");
    for(const auto* replacement:{"-1","1.5","true","\"1\""}) {
        auto text=SerializeFrame(Fixture());const auto pos=text.find("\"id\":1");text.replace(pos+5,1,replacement);
        Reject([&]{DeserializeFrame(text);},"invalid ID representation rejected");
    }
}
class CountingSource final : public IFrameSource {
    std::uint64_t sequence_{};
public:
    SourceRead Poll(const ReadContext&) override {
        auto f=Fixture(++sequence_);f.camera.position.x=static_cast<float>(sequence_);
        for(auto& e:f.entities) e.position.x=static_cast<float>(sequence_);
        return {ReadState::Ready,std::move(f),{}};
    }
};
void Concurrency() {
    RuntimeOptions o;o.pollMs=5;
    RuntimeDataProvider provider(std::make_unique<CountingSource>(),o);
    auto held=Wait(provider,[](const RuntimeSnapshot& s){return s.diagnostics.acceptedFrames>=1;});
    const auto original=SerializeFrame(*held->frame);
    std::atomic<bool> failed{};std::vector<std::thread> readers;
    for(int thread=0;thread<4;++thread) readers.emplace_back([&] {
        std::uint64_t previous{};
        for(int i=0;i<500;++i) {
            provider.SetContext(16.0f/9.0f,CameraSettings{i%2?60.0f:120.0f});
            const auto snapshot=provider.Latest();const auto& f=*snapshot->frame;
            if(!f.entities.empty()) {
                if(f.sequence<previous||f.camera.position.x!=static_cast<float>(f.sequence)) failed=true;
                for(const auto& e:f.entities) if(e.position.x!=f.camera.position.x) failed=true;
                previous=f.sequence;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });
    for(auto& t:readers) t.join();
    Check(!failed,"concurrent readers observed a torn or regressed snapshot");
    Check(provider.Latest()->diagnostics.acceptedFrames>3,"worker published during concurrent reads");
    Check(SerializeFrame(*held->frame)==original,"retained snapshot mutated after publication");
    std::thread first([&]{provider.Stop();}),second([&]{provider.Stop();});first.join();second.join();
    Check(provider.Latest()->diagnostics.state==SourceState::Stopped&&provider.Latest()->frame->entities.empty(),"concurrent stop clears output and joins safely");
}
class SlowSource final : public IFrameSource {
    bool first_=true;
public:
    std::atomic<bool> waiting{};
    SourceRead Poll(const ReadContext&) override {
        if(first_) {first_=false;return {ReadState::Ready,Fixture(),{}};}
        waiting=true;std::this_thread::sleep_for(std::chrono::milliseconds(250));
        return {ReadState::Disconnected,{},"delayed source"};
    }
};
void Recovery() {
    Temp temp;const auto path=temp.path/"frame.json";auto o=FileOptions();
    RuntimeDataProvider p(std::make_unique<JsonFileSource>(path),o);
    Wait(p,[](const RuntimeSnapshot& s){return s.diagnostics.state==SourceState::Reconnecting&&s.frame->entities.empty();});
    Write(path,SerializeFrame(Fixture(1)));
    Wait(p,[](const RuntimeSnapshot& s){return s.frame->sequence==1;});
    auto fewer=Fixture(2,1);Write(path,SerializeFrame(fewer));
    Wait(p,[](const RuntimeSnapshot& s){return s.frame->sequence==2&&s.frame->entities.size()==1;});
    Wait(p,[](const RuntimeSnapshot& s){return s.diagnostics.state==SourceState::Reconnecting&&s.frame->entities.empty();});
    std::filesystem::remove(path);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Write(path,SerializeFrame(Fixture(2)));
    Wait(p,[](const RuntimeSnapshot& s){return s.frame->sequence==2&&s.frame->entities.size()==11;});
    Write(path,"{broken");
    Wait(p,[](const RuntimeSnapshot& s){return s.diagnostics.state==SourceState::Invalid&&s.frame->entities.empty();});
    Write(path,SerializeFrame(Fixture(3,0)));
    Wait(p,[](const RuntimeSnapshot& s){return s.frame->sequence==3&&s.frame->entities.empty()&&s.diagnostics.state==SourceState::Live;});
    p.Stop();
    auto slow=std::make_unique<SlowSource>();auto* slowPointer=slow.get();
    RuntimeDataProvider delayed(std::move(slow),o);
    Wait(delayed,[&](const RuntimeSnapshot& s){return slowPointer->waiting.load()&&s.diagnostics.state==SourceState::Reconnecting&&s.frame->entities.empty();});
    delayed.Stop();
    std::filesystem::remove(path);o.fallbackDemo=true;
    RuntimeDataProvider fallback(std::make_unique<JsonFileSource>(path),o);
    const auto demo=Wait(fallback,[](const RuntimeSnapshot& s){return s.diagnostics.fallback;});
    Check(demo->frame->source.find("DEMO FALLBACK")!=std::string::npos,"fallback explicitly labeled");
    Write(path,SerializeFrame(Fixture(4)));
    Wait(fallback,[](const RuntimeSnapshot& s){return !s.diagnostics.fallback&&s.frame->sequence==4&&s.diagnostics.state==SourceState::Live;});
}
void Recording() {
    Temp temp;const auto path=temp.path/"capture.jsonl";
    {
        FrameRecorder writer(path);const auto now=Clock::now();writer.Write(Fixture(1),now);writer.Write(Fixture(2),now+std::chrono::milliseconds(50));
        Reject([&]{writer.Write(Fixture(3),now);},"recorder rejects regressed time");
    }
    ReplaySource replay(path,false);ReadContext c;
    Check(replay.Poll(c).frame->sequence==1,"first replay frame");
    Check(replay.Poll(c).state==ReadState::Scheduled,"replay honors recorded timing");
    c.now+=std::chrono::milliseconds(100);Check(replay.Poll(c).frame->sequence==2,"second timed replay frame");
    Check(replay.Poll(c).state==ReadState::Ended,"nonlooping replay ends");
    {FrameRecorder writer(path);writer.Write(Fixture(3),Clock::now());}
    Check(replay.Poll(c).frame->sequence==1,"changed replay reopens");
    c.now+=std::chrono::seconds(1);Check(replay.Poll(c).frame->sequence==2,"appended recording keeps first session");
    Check(replay.Poll(c).frame->sequence==3,"appended session starts a new clock");
    ReplaySource loop(path,true);Check(loop.Poll(c).frame->sequence==1,"loop begins");c.now+=std::chrono::seconds(1);
    (void)loop.Poll(c);(void)loop.Poll(c);Check(loop.Poll(c).state==ReadState::Scheduled,"loop schedules restart");
    Check(loop.Poll(c).frame->sequence==1,"loop repeats first frame");
    const auto bad=temp.path/"bad.jsonl";Write(bad,"{\n");ReplaySource invalid(bad,false);
    Check(invalid.Poll(c).state==ReadState::Invalid,"malformed recording rejected");
    Reject([&]{FrameRecorder writer(bad);},"recorder refuses unrelated or invalid destination");
    std::filesystem::remove(bad);Check(invalid.Poll(c).state==ReadState::Disconnected,"missing replay detected");
    std::filesystem::copy_file(path,bad);Check(invalid.Poll(c).state==ReadState::Ready,"replay recovers when file returns");
    auto options=FileOptions();options.jsonPath="same.json";options.record=true;options.recordingPath="same.json";
    Reject([&]{MakeFrameSource(options,temp.path);},"recording cannot overwrite input");
    options=RuntimeOptions{};options.pollMs=5;options.record=true;
    const auto automatic=temp.path/"automatic.jsonl";
    {RuntimeDataProvider provider(std::make_unique<CountingSource>(),options,automatic);
     Wait(provider,[](const RuntimeSnapshot& s){return s.diagnostics.acceptedFrames>=3&&s.diagnostics.recording=="RECORDING";});}
    ReplaySource recorded(automatic,false);Check(recorded.Poll(c).state==ReadState::Ready,"runtime recording is playable");
    RuntimeDataProvider unwritable(std::make_unique<CountingSource>(),options,temp.path/"missing"/"capture.jsonl");
    Wait(unwritable,[](const RuntimeSnapshot& s){return s.diagnostics.state==SourceState::Live&&s.diagnostics.recording.find("RECORD ERROR")==0;});
}
void Scale() {
    const auto f=Fixture(1,MaxFrameEntities);const auto text=SerializeFrame(f);const auto decoded=DeserializeFrame(text);
    Check(decoded.entities.size()==MaxFrameEntities&&decoded.entities.back().id==MaxFrameEntities,"10000-object snapshot round trip");
    Temp temp;const auto path=temp.path/"large.json";Write(path,text);JsonFileSource source(path);ReadContext c;
    Check(source.Poll(c).frame->entities.size()==MaxFrameEntities,"large file source accepted");
    Check(source.Poll(c).state==ReadState::Unchanged,"unchanged large file is not decoded again");
}
}
int main(int argc,char** argv) {
    try {
        Check(argc==2,"expected test group");const std::string group=argv[1];
        if(group=="serialization") Serialization();else if(group=="validation") Validation();else if(group=="concurrency") Concurrency();
        else if(group=="recovery") Recovery();else if(group=="recording") Recording();else if(group=="scale") Scale();
        else throw std::runtime_error("unknown group");
        std::cout<<"PASS: "<<group<<'\n';return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
