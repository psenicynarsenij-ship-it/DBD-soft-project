#pragma once
#include "math.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace esp {
enum class EntityKind { Survivor, Killer, Generator, Hook, ExitGate };
struct Entity {
    std::uint64_t id{};
    EntityKind kind{};
    std::string name; // UTF-8
    Vec3 position;    // Feet or marker anchor, in meters.
    Vec3 top;         // Head/top point for character boxes, in meters.
    std::optional<Vec3> lookDirection; // Optional normalized world-space direction.
};
struct CameraSettings {
    static constexpr float MinFovDegrees=60, MaxFovDegrees=120;
    float verticalFovDegrees=60;
};
struct Camera { Vec3 position; Matrix4 viewProjection; };
struct Frame { Camera camera; std::vector<Entity> entities; std::string source; };
class IDataProvider {
public:
    virtual ~IDataProvider() = default;
    // Return one consistent snapshot. Implementations own any synchronization.
    virtual Frame Sample(float viewportAspect, const CameraSettings&) = 0;
};
}
