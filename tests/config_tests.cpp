#include "esp/config.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

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
        std::string status;
        Require(esp::SaveConfig(configPath,original,status),"save config");
        esp::VisualConfig loaded;
        Require(esp::LoadConfig(configPath,loaded,status),"load config");
        Require(!loaded.hooks&&loaded.maxDistance==87&&loaded.killerColor==original.killerColor,"round trip values");
        original.maxDistance=99;
        Require(esp::SaveConfig(configPath,original,status),"overwrite existing config");
        Require(esp::LoadConfig(configPath,loaded,status)&&loaded.maxDistance==99,"load overwritten config");
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
        std::cout<<"PASS: config persistence, overwrite, invalid input preservation and explicit provider selection\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
