#pragma once
#include "esp/math.hpp"
#include <cstdint>
#include <cstddef>
#include <nlohmann/json.hpp>
namespace fixture {
inline constexpr std::uint64_t Magic=0x31304D5246505345ULL;
inline constexpr std::uint32_t Capacity=64;
struct Entity {
    std::uint64_t id;std::uint32_t kind,flags;
    esp::Vec3 position,top,direction;char name[64];
};
struct State {
    std::uint64_t magic;std::uint32_t version,capacity;
    std::uint64_t revision,sequence,timestamp;
    std::uint32_t count,headerSize;
    esp::Vec3 camera;esp::Matrix4 matrix;std::uint32_t reserved;
    Entity entities[Capacity];
};
static_assert(sizeof(Entity)==120 && offsetof(State,entities)==128);
inline nlohmann::json Layout(std::uint32_t imageSize,std::uint32_t timestamp) {
    return {{"schema",1},{"coordinate_contract","row-major-column-vector-lh-y-up-dx01"},
      {"verified_build","EspMemoryFixture contract v1"},{"executable","EspMemoryFixture.exe"},{"module","EspMemoryFixture.exe"},
      {"image_size",imageSize},{"pe_timestamp",timestamp},{"machine","AMD64"},{"root_export","EspFixtureState"},{"pointer_chain",{0}},
      {"max_entities",Capacity},{"entities_offset",offsetof(State,entities)},
      {"header",{{"size",offsetof(State,entities)},{"magic_value",Magic},{"version_value",1},
       {"magic",offsetof(State,magic)},{"version",offsetof(State,version)},{"revision",offsetof(State,revision)},
       {"sequence",offsetof(State,sequence)},{"timestamp_us",offsetof(State,timestamp)},{"count",offsetof(State,count)},
       {"camera_position",offsetof(State,camera)},{"view_projection",offsetof(State,matrix)}}},
      {"entity",{{"stride",sizeof(Entity)},{"id",offsetof(Entity,id)},{"kind",offsetof(Entity,kind)},
       {"flags",offsetof(Entity,flags)},{"direction_mask",1},{"position",offsetof(Entity,position)},
       {"top",offsetof(Entity,top)},{"direction",offsetof(Entity,direction)},{"name",offsetof(Entity,name)},
       {"name_bytes",64},{"kinds",{0,1,2,3,4}}}}};
}
}
