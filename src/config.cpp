#include "esp/config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>
#include <stdexcept>
namespace esp {
using nlohmann::json;
static void Validate(const VisualConfig& c) {
    if(!std::isfinite(c.maxDistance)||c.maxDistance<1||c.maxDistance>1000) throw std::runtime_error("max_distance must be 1..1000");
    for(const auto& color:{c.survivorColor,c.killerColor}) for(float v:color)
        if(!std::isfinite(v)||v<0||v>1) throw std::runtime_error("color channels must be 0..1");
}
bool LoadConfig(const std::filesystem::path& path,VisualConfig& c,std::string& status) {
    try {
        if(!std::filesystem::exists(path)) {status="No config.json: defaults active";return false;}
        std::ifstream in(path); if(!in) throw std::runtime_error("cannot open config.json");
        const auto j=json::parse(in);
        if(j.at("schema_version")!=1) throw std::runtime_error("unsupported config schema");
        VisualConfig next;
        const auto& v=j.at("visuals");
        next.enabled=v.at("enabled"); next.survivors=v.at("survivors"); next.killer=v.at("killer");
        next.generators=v.at("generators"); next.hooks=v.at("hooks"); next.exitGates=v.at("exit_gates");
        next.names=v.at("names"); next.distances=v.at("distances"); next.maxDistance=v.at("max_distance");
        next.survivorColor=v.at("survivor_color").get<std::array<float,4>>();
        next.killerColor=v.at("killer_color").get<std::array<float,4>>();
        Validate(next); c=next; status="Loaded config.json";return true;
    } catch(const std::exception& e) { status=std::string("Load failed: ")+e.what();return false; }
}
bool SaveConfig(const std::filesystem::path& path,const VisualConfig& c,std::string& status) {
    try {
        Validate(c);
        const json j={{"schema_version",1},{"visuals",{
            {"enabled",c.enabled},{"survivors",c.survivors},{"killer",c.killer},
            {"generators",c.generators},{"hooks",c.hooks},{"exit_gates",c.exitGates},
            {"names",c.names},{"distances",c.distances},{"max_distance",c.maxDistance},
            {"survivor_color",c.survivorColor},{"killer_color",c.killerColor}}}};
        auto temporary=path; temporary+=".tmp";
        {std::ofstream out(temporary,std::ios::trunc);out.exceptions(std::ios::failbit|std::ios::badbit);out<<j.dump(2)<<'\n';out.close();}
        // Copy-over preserves the old file until a complete temporary file exists.
        std::filesystem::copy_file(temporary,path,std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(temporary);
        status="Saved config.json";return true;
    } catch(const std::exception& e) {status=std::string("Save failed: ")+e.what();return false;}
}
bool ValidateOffsets(const std::filesystem::path& path,std::string& status) {
    try {
        std::ifstream in(path); if(!in) throw std::runtime_error("offsets.json not found");
        const auto j=json::parse(in);
        if(j.at("schema_version")!=1||j.at("target_version")!="10.2.2") throw std::runtime_error("unsupported offsets schema/version");
        if(j.at("provider")!="demo") throw std::runtime_error("only demo provider is implemented");
        status="offsets.json: DEMO; real addresses are not configured";return true;
    } catch(const std::exception& e) {status=std::string("Offsets: ")+e.what();return false;}
}
}
