#pragma once
#include <array>
#include <filesystem>
#include <string>
#include "data_provider.hpp"
namespace esp {
struct VisualConfig {
    bool enabled=true, survivors=true, killer=true, generators=true, hooks=true, exitGates=true;
    bool names=true, distances=true;
    float maxDistance=150;
    std::array<float,4> survivorColor{0.2f,0.9f,1,1};
    std::array<float,4> killerColor{1,0.2f,0.25f,1};
    CameraSettings camera;
    bool killerDirection=true, killerCone=false;
    float killerConeDegrees=90, killerArrowLength=6;
    std::array<float,3> killerArrowColor{1,0.55f,0.15f};
    float killerArrowOpacity=0.9f, killerConeOpacity=0.15f;
};
bool LoadConfig(const std::filesystem::path&,VisualConfig&,std::string& status);
bool SaveConfig(const std::filesystem::path&,const VisualConfig&,std::string& status);
bool ValidateOffsets(const std::filesystem::path&,std::string& status);
}
