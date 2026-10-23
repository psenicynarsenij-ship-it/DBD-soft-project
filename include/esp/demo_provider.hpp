#pragma once
#include "data_provider.hpp"
#include <chrono>
namespace esp {
class DemoProvider final : public IDataProvider {
    std::chrono::steady_clock::time_point start_=std::chrono::steady_clock::now();
public:
    Frame Sample(float viewportAspect, const CameraSettings&) override;
    // Deterministic demo snapshot for tests; no wall-clock sleeps required.
    Frame SampleAt(float viewportAspect, const CameraSettings&, float elapsedSeconds) const;
};
}
