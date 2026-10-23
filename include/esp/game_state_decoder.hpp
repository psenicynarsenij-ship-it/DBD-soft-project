#pragma once
#include "frame_sources.hpp"
#include "process_reader.hpp"
#include <nlohmann/json.hpp>
namespace esp {
// This descriptor describes a verified producer contract, never inferred DBD offsets.
struct MemoryLayout {
    std::string executable,module,build,rootExport;
    std::uint32_t pid{},imageSize{},peTimestamp{};
    std::uint64_t rootRva{},magic{};
    std::vector<std::uint64_t> pointerChain;
    std::uint32_t version{},headerSize{},stride{},maxEntities{};
    std::uint32_t magicOffset{},versionOffset{},revisionOffset{},sequenceOffset{},timestampOffset{},countOffset{},cameraOffset{},matrixOffset{};
    std::uint32_t entitiesOffset{},idOffset{},kindOffset{},positionOffset{},topOffset{},directionOffset{},flagsOffset{},nameOffset{},nameBytes{},directionMask{};
    bool entitiesPointer=false;
    std::vector<std::uint32_t> kinds;
    static MemoryLayout Parse(const nlohmann::json&);
};
class GameStateDecoder {
    MemoryLayout layout_;
public:
    explicit GameStateDecoder(MemoryLayout layout):layout_(std::move(layout)){}
    std::optional<Frame> Read(const IMemoryReader&,std::uint64_t root) const;
};
class LiveFrameSource final : public IFrameSource {
    std::filesystem::path path_;
    ProcessReader process_;
    std::optional<MemoryLayout> layout_;
    std::optional<std::filesystem::file_time_type> modified_;
    std::optional<std::uint64_t> sequence_;
    std::uint64_t root_{};
public:
    explicit LiveFrameSource(std::filesystem::path path):path_(std::move(path)){}
    SourceRead Poll(const ReadContext&) override;
};
}
