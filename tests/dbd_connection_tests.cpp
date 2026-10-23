#include "esp/runtime_provider.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
namespace {
void Check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
void Disconnected(const esp::DbdConnectionStatus& status) {
    Check(!status.connected&&!status.adapterImplemented&&!status.liveSessionValidated,"transport incorrectly authenticated a DBD session");
    Check(status.state=="NOT CONNECTED"&&status.targetVersion=="10.2.2","honest status/version label required");
}
struct Temp {
    std::filesystem::path path=std::filesystem::temp_directory_path()/("dbd-provenance-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() {std::filesystem::create_directory(path);}
    ~Temp() {std::error_code ec;std::filesystem::remove_all(path,ec);}
};
template<class Predicate> auto Wait(esp::RuntimeDataProvider& p,Predicate predicate) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(std::chrono::steady_clock::now()<deadline) {
        const auto snapshot=p.Latest();Disconnected(snapshot->diagnostics.dbd);
        if(predicate(*snapshot)) return snapshot;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("timed out waiting for transport state");
}
}
int main() {
    try {
        for(const auto kind:{esp::SourceKind::Demo,esp::SourceKind::JsonFile,esp::SourceKind::Replay,esp::SourceKind::LiveMemory}) {
            Disconnected(esp::DescribeDbdConnection(kind));Disconnected(esp::DescribeDbdConnection(kind,true));
        }
        const auto capability=nlohmann::json::parse(esp::DbdBuildStatusJson());
        Check(capability.at("scope")=="binary_capabilities"&&capability.at("missing_feed_fields").size()==8,"capabilities distinguish missing inputs from game data");
        Temp temp;const auto path=temp.path/"spoofed.json";
        esp::DemoProvider demo;auto frame=demo.SampleAt(16.0f/9.0f,esp::CameraSettings{90},0);
        frame.sequence=1;frame.source="VERIFIED LIVE DBD 10.2.2";
        auto json=nlohmann::json::parse(esp::SerializeFrame(frame));
        json["connected"]=true;json["dbd"]={{"verified",true},{"target_version","10.2.2"}};
        esp::AtomicWriteText(path,json.dump());
        esp::RuntimeOptions options;options.kind=esp::SourceKind::JsonFile;options.pollMs=5;options.staleMs=100;
        esp::RuntimeDataProvider file(std::make_unique<esp::JsonFileSource>(path),options);
        const auto live=Wait(file,[](const esp::RuntimeSnapshot& s){return s.diagnostics.state==esp::SourceState::Live;});
        Check(live->frame->entities.size()==11&&!live->diagnostics.fallback,"JSON transport still works");
        Check(live->frame->source==esp::SourceKindName(esp::SourceKind::JsonFile),"untrusted source label cannot claim a verified connection");
        std::filesystem::remove(path);
        Wait(file,[](const esp::RuntimeSnapshot& s){return s.diagnostics.state==esp::SourceState::Reconnecting&&s.frame->entities.empty();});file.Stop();
        Disconnected(file.Latest()->diagnostics.dbd);
        options.fallbackDemo=true;
        esp::RuntimeDataProvider fallback(std::make_unique<esp::JsonFileSource>(path),options);
        const auto replacement=Wait(fallback,[](const esp::RuntimeSnapshot& s){return s.diagnostics.fallback;});
        Check(replacement->diagnostics.dbd.reason.find("Synthetic fallback")!=std::string_view::npos,"fallback diagnostic must remain synthetic");
        std::cout<<"PASS: all transports remain NOT CONNECTED; spoofed JSON, source loss and demo fallback cannot authenticate DBD\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
