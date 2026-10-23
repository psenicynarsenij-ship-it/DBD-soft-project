#include <filesystem>
#include "esp/overlay.hpp"
#include "esp/runtime_provider.hpp"
#include "esp/renderer.hpp"
#include <array>
#include <memory>
#include <exception>
#include <stdexcept>

namespace {
class Keys {
    std::array<bool,256> previous_{};
public:
    bool Pressed(int key) {
        const bool down=(GetAsyncKeyState(key)&0x8000)!=0;
        const bool edge=down&&!previous_[key];previous_[key]=down;return edge;
    }
};
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    try {
        const auto directory=esp::ExecutableDirectory();
        const auto configPath=directory/L"config.json";
        esp::VisualConfig config;std::string status,offsetsStatus;
        esp::LoadConfig(configPath,config,status);
        // Fail clearly if the data-source configuration is invalid or requests a real provider.
        if(!esp::ValidateOffsets(directory/L"offsets.json",offsetsStatus)) throw std::runtime_error(offsetsStatus);
        esp::Overlay overlay;overlay.Initialize(instance);
        std::unique_ptr<esp::RuntimeDataProvider> provider;
        auto startSource=[&] {
            auto source=esp::MakeFrameSource(config.runtime,directory);
            if(provider) provider->Stop();
            provider=std::make_unique<esp::RuntimeDataProvider>(std::move(source),config.runtime,
                esp::ResolveDataPath(directory,config.runtime.recordingPath),
                esp::ReadContext{overlay.Width()/overlay.Height(),config.camera,std::chrono::steady_clock::now()});
        };
        startSource();
        bool menuOpen=true,restartSource=false;overlay.SetInteractive(menuOpen);Keys keys;
        while(overlay.Poll()) {
            if(keys.Pressed(VK_END)) break;
            if(keys.Pressed(VK_INSERT)) menuOpen=!menuOpen;
            bool* toggles[]={&config.enabled,&config.survivors,&config.killer,&config.generators,&config.hooks,&config.exitGates,&config.names,&config.distances};
            for(int i=0;i<8;++i) if(keys.Pressed(VK_F1+i)) *toggles[i]=!*toggles[i];
            overlay.SetInteractive(menuOpen);
            if(!overlay.BeginFrame()) continue;
            if(restartSource) {
                restartSource=false;
                try {startSource();status="Source settings applied";}
                catch(const std::exception& e) {status=std::string("Source settings failed: ")+e.what();}
            }
            provider->SetContext(overlay.Width()/overlay.Height(),config.camera);
            const auto snapshot=provider->Latest();
            esp::RenderEntities(*snapshot->frame,config,overlay.Width(),overlay.Height(),snapshot->diagnostics);
            if(menuOpen) esp::RenderMenu(config,menuOpen,configPath,status,offsetsStatus,restartSource);
            overlay.SetInteractive(menuOpen);overlay.EndFrame();
        }
        return 0;
    } catch(const std::exception& e) {
        MessageBoxA(nullptr,e.what(),"Visual ESP - startup/render error",MB_OK|MB_ICONERROR);return 1;
    }
}
