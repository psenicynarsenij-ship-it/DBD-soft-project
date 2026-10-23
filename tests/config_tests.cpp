#include "esp/config.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <nlohmann/json.hpp>

namespace {
void Require(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
struct TemporaryDirectory {
    std::filesystem::path path=std::filesystem::temp_directory_path()/
        ("esp-config-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryDirectory() {std::filesystem::create_directory(path);}
    ~TemporaryDirectory() {std::error_code ec;std::filesystem::remove_all(path,ec);}
};
}
int main() {
    try {
        TemporaryDirectory temp;
        const auto configPath=temp.path/"config.json";
        esp::VisualConfig original;original.hooks=false;original.maxDistance=87;original.killerColor={0.1f,0.2f,0.3f,0.4f};
        original.camera.verticalFovDegrees=110;
        original.killerDirection=false;original.killerCone=true;original.killerConeDegrees=100;
        original.killerArrowLength=12;original.killerArrowColor={0.3f,0.4f,0.5f};
        original.killerArrowOpacity=0.6f;original.killerConeOpacity=0.2f;
        std::string status;
        Require(esp::SaveConfig(configPath,original,status),"save config");
        esp::VisualConfig loaded;
        Require(esp::LoadConfig(configPath,loaded,status),"load config");
        Require(!loaded.hooks&&loaded.maxDistance==87&&loaded.killerColor==original.killerColor,"round trip values");
        Require(loaded.camera.verticalFovDegrees==110&&!loaded.killerDirection&&loaded.killerCone&&
                loaded.killerConeDegrees==100&&loaded.killerArrowLength==12&&loaded.killerArrowColor==original.killerArrowColor&&
                loaded.killerArrowOpacity==0.6f&&loaded.killerConeOpacity==0.2f,"all new settings round trip");
        original.maxDistance=99;
        Require(esp::SaveConfig(configPath,original,status),"overwrite existing config");
        Require(esp::LoadConfig(configPath,loaded,status)&&loaded.maxDistance==99,"load overwritten config");
        nlohmann::json saved;
        {std::ifstream in(configPath);in>>saved;}
        auto legacy=saved;legacy.erase("camera");
        for(const auto* field:{"killer_direction","killer_cone","killer_cone_degrees","killer_arrow_length","killer_arrow_color","killer_arrow_opacity","killer_cone_opacity"})
            legacy["visuals"].erase(field);
        {std::ofstream out(configPath);out<<legacy;}
        Require(esp::LoadConfig(configPath,loaded,status)&&loaded.camera.verticalFovDegrees==60&&loaded.killerDirection&&
                !loaded.killerCone&&loaded.killerConeDegrees==90&&loaded.killerArrowLength==6,"legacy config uses new defaults");
        for(const auto* field:{"killer_cone_degrees","killer_arrow_length","killer_arrow_opacity","killer_cone_opacity"}) {
            auto invalid=saved;invalid["visuals"][field]=-1;
            {std::ofstream out(configPath);out<<invalid;}
            Require(!esp::LoadConfig(configPath,loaded,status)&&loaded.camera.verticalFovDegrees==60,"invalid new field preserves current config");
        }
        for(float fov:{59.0f,121.0f}) {
            auto invalid=saved;invalid["camera"]["vertical_fov_degrees"]=fov;
            {std::ofstream out(configPath);out<<invalid;}
            Require(!esp::LoadConfig(configPath,loaded,status),"invalid camera FOV rejected on load");
        }
        for(float fov:{60.0f,120.0f}) for(float cone:{30.0f,120.0f}) {
            auto boundary=original;boundary.camera.verticalFovDegrees=fov;boundary.killerConeDegrees=cone;
            Require(esp::SaveConfig(configPath,boundary,status),"valid FOV and cone boundaries accepted");
        }
        auto nan=original;nan.camera.verticalFovDegrees=std::numeric_limits<float>::quiet_NaN();
        Require(!esp::SaveConfig(configPath,nan,status),"nonfinite FOV rejected before serialization");
        auto badColor=original;badColor.killerArrowColor[0]=1.1f;
        Require(!esp::SaveConfig(configPath,badColor,status),"invalid arrow color rejected");
        {std::ofstream out(configPath);out<<R"({"schema_version":1,"visuals":{}})";}
        Require(!esp::LoadConfig(configPath,loaded,status)&&loaded.maxDistance==99,"invalid config preserves state");
        Require(!esp::LoadConfig(temp.path/"missing.json",loaded,status),"missing config reported");
        original.maxDistance=-1;
        Require(!esp::SaveConfig(configPath,original,status),"invalid distance rejected");
        const auto offsets=temp.path/"offsets.json";
        {std::ofstream out(offsets);out<<R"({"schema_version":1,"target_version":"10.2.2","provider":"demo"})";}
        Require(esp::ValidateOffsets(offsets,status),"demo config accepted");
        {std::ofstream out(offsets);out<<R"({"schema_version":1,"target_version":"10.2.2","provider":"real"})";}
        Require(!esp::ValidateOffsets(offsets,status),"unimplemented real source rejected");
        Require(!esp::ValidateOffsets(temp.path/"missing-offsets.json",status),"missing offsets rejected");
        std::cout<<"PASS: new camera/direction config round trip, legacy migration, bounds, invalid input preservation and provider selection\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
