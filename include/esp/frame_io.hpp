#pragma once
#include "data_provider.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace esp {
inline constexpr std::size_t MaxFrameEntities=10000, MaxFrameBytes=8*1024*1024;
inline constexpr std::uintmax_t MaxRecordingBytes=256*1024*1024;
void ValidateFrame(const Frame&); // Rejects the entire snapshot on any invalid field.
std::string SerializeFrame(const Frame&);
Frame DeserializeFrame(std::string_view);
void AtomicWriteText(const std::filesystem::path&,std::string_view); // One publisher per path.
struct RecordedFrame { Frame frame; std::uint64_t elapsedUs{}; std::string session; };
RecordedFrame DeserializeRecording(std::string_view);
class FrameRecorder {
    std::ofstream out_;
    std::filesystem::path path_;
    std::string session_;
    std::chrono::steady_clock::time_point start_;
    std::uintmax_t bytes_{};
    std::uint64_t lastElapsed_{};
public:
    explicit FrameRecorder(const std::filesystem::path&);
    void Write(const Frame&,std::chrono::steady_clock::time_point);
};
}
