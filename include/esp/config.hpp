#pragma once
#include <array>
#include <filesystem>
#include <string>
namespace esp {
struct VisualConfig {
    bool enabled=true, survivors=true, killer=true, generators=true, hooks=true, exitGates=true;
    bool names=true, distances=true;
    float maxDistance=150;
    std::array<float,4> survivorColor{0.2f,0.9f,1,1};
    std::array<float,4> killerColor{1,0.2f,0.25f,1};
};
bool LoadConfig(const std::filesystem::path&,VisualConfig&,std::string& status);
bool SaveConfig(const std::filesystem::path&,const VisualConfig&,std::string& status);
bool ValidateOffsets(const std::filesystem::path&,std::string& status);
}
