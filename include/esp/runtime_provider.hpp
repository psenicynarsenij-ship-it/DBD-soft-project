#pragma once
#include "frame_sources.hpp"
#include "dbd_connection.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace esp {
enum class SourceState { Starting, Live, Reconnecting, Invalid, Ended, Stopped };
const char* SourceStateName(SourceState);
struct RuntimeDiagnostics {
    SourceState state=SourceState::Starting;
    SourceKind kind=SourceKind::Demo;
    bool fallback{};
    DbdConnectionStatus dbd;
    std::uint64_t acceptedFrames{}, invalidFrames{};
    double acquisitionMs{}, sourceFps{}, ageMs{};
    std::string message, recording;
    std::chrono::steady_clock::time_point measuredAt=std::chrono::steady_clock::now();
};
struct RuntimeSnapshot {
    std::shared_ptr<const Frame> frame=std::make_shared<const Frame>();
    RuntimeDiagnostics diagnostics;
    std::optional<std::chrono::steady_clock::time_point> validUntil;
};
class RuntimeDataProvider final : public IDataProvider {
    RuntimeOptions options_;
    std::filesystem::path recordingPath_;
    std::unique_ptr<IFrameSource> source_;
    std::atomic<std::shared_ptr<const RuntimeSnapshot>> latest_;
    std::mutex mutex_, stopMutex_;
    std::condition_variable wake_;
    ReadContext context_;
    bool stop_{};
    std::thread worker_;
    void Run();
public:
    RuntimeDataProvider(std::unique_ptr<IFrameSource>,RuntimeOptions,std::filesystem::path recordingPath={},ReadContext initialContext={});
    ~RuntimeDataProvider() override;
    RuntimeDataProvider(const RuntimeDataProvider&)=delete;
    RuntimeDataProvider& operator=(const RuntimeDataProvider&)=delete;
    void SetContext(float aspect,const CameraSettings&);
    std::shared_ptr<const RuntimeSnapshot> Latest() const;
    Frame Sample(float aspect,const CameraSettings&) override; // Compatibility copy.
    void Stop();
};
}
