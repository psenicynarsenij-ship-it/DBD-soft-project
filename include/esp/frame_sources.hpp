#pragma once
#include "frame_io.hpp"
#include "demo_provider.hpp"
#include <memory>

namespace esp {
enum class SourceKind { Demo, JsonFile, Replay, LiveMemory };
struct RuntimeOptions {
    SourceKind kind=SourceKind::Demo;
    std::string jsonPath="frames.json", replayPath="recording.jsonl", recordingPath="capture.jsonl";
    std::string memoryLayoutPath="memory.fixture.json";
    bool fallbackDemo=false, replayLoop=true, record=false;
    int pollMs=16, staleMs=1000;
    bool operator==(const RuntimeOptions&) const = default;
};
void ValidateRuntimeOptions(const RuntimeOptions&);
const char* SourceKindName(SourceKind);
std::filesystem::path ResolveDataPath(const std::filesystem::path&,const std::string&);
enum class ReadState { Ready, Unchanged, Scheduled, Disconnected, Invalid, Ended };
struct SourceRead {
    ReadState state=ReadState::Unchanged;
    std::optional<Frame> frame;
    std::string message;
};
struct ReadContext {
    float aspect=16.0f/9.0f;
    CameraSettings camera;
    std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now();
};
class IFrameSource {
public:
    virtual ~IFrameSource()=default;
    // Worker thread only. Poll must return in bounded time; it never touches a renderer.
    virtual SourceRead Poll(const ReadContext&)=0;
};
class DemoFrameSource final : public IFrameSource {
    DemoProvider demo_;
    std::uint64_t sequence_{};
public:
    SourceRead Poll(const ReadContext&) override;
};
class JsonFileSource final : public IFrameSource {
    std::filesystem::path path_;
    std::optional<std::filesystem::file_time_type> modified_;
    std::optional<std::uint64_t> sequence_;
    std::uintmax_t size_{};
    std::string invalid_;
public:
    explicit JsonFileSource(std::filesystem::path path):path_(std::move(path)) {}
    SourceRead Poll(const ReadContext&) override;
};
class ReplaySource final : public IFrameSource {
    std::filesystem::path path_;
    bool loop_{};
    std::uintmax_t size_{};
    std::ifstream in_;
    std::optional<RecordedFrame> pending_;
    std::optional<std::filesystem::file_time_type> modified_;
    std::string session_;
    std::chrono::steady_clock::time_point start_;
    std::uint64_t previousElapsed_{}, firstElapsed_{};
    bool ended_{};
    void Reset();
public:
    ReplaySource(std::filesystem::path path,bool loop):path_(std::move(path)),loop_(loop) {}
    SourceRead Poll(const ReadContext&) override;
};
std::unique_ptr<IFrameSource> MakeFrameSource(const RuntimeOptions&,const std::filesystem::path& base);
}
