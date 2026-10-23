#include "esp/renderer.hpp"
#include "esp/demo_provider.hpp"
#include <imgui.h>
#include <iostream>
#include <stdexcept>
namespace {
void Check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
}
int main() {
    try {
        IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;
        io.DisplaySize={1920,1080};io.DeltaTime=1.0f/60;
        unsigned char* pixels{};int width{},height{};io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        esp::DemoProvider demo;auto frame=demo.SampleAt(16.0f/9.0f,esp::CameraSettings{90},0);
        const auto entity=frame.entities.at(5);frame.entities.clear();
        for(std::size_t i=0;i<esp::MaxFrameEntities;++i) {auto e=entity;e.id=i+1;frame.entities.push_back(e);}
        esp::ValidateFrame(frame);
        esp::VisualConfig config;config.maxDrawEntities=25;esp::RuntimeDiagnostics diagnostics;
        ImGui::NewFrame();const auto stats=esp::RenderEntities(frame,config,1920,1080,diagnostics);ImGui::Render();
        Check(stats.total==10000&&stats.visible==10000&&stats.drawn==25,"visible set respects draw budget");
        Check(ImGui::GetDrawData()->TotalVtxCount>0,"CPU ImGui draw commands produced");
        config.enabled=false;ImGui::NewFrame();const auto off=esp::RenderEntities(frame,config,1920,1080,diagnostics);ImGui::Render();
        Check(off.drawn==0,"master toggle hides all objects");
        config.enabled=true;config.generators=false;
        ImGui::NewFrame();Check(esp::RenderEntities(frame,config,1920,1080,diagnostics).drawn==0,"category toggle culls objects");ImGui::Render();
        frame=demo.SampleAt(16.0f/9.0f,esp::CameraSettings{90},2);
        const auto killer=frame.entities.at(4);frame.entities={killer};config.killerDirection=false;config.killerCone=false;
        ImGui::NewFrame();esp::RenderEntities(frame,config,1920,1080,diagnostics);
        const auto boxes=ImGui::GetBackgroundDrawList()->VtxBuffer.Size;ImGui::Render();
        config.killerDirection=true;config.killerCone=true;
        ImGui::NewFrame();esp::RenderEntities(frame,config,1920,1080,diagnostics);
        const auto withView=ImGui::GetBackgroundDrawList()->VtxBuffer.Size;ImGui::Render();
        Check(withView>boxes,"killer arrow and cone produce projected geometry");
        frame.entities.front().lookDirection.reset();
        ImGui::NewFrame();esp::RenderEntities(frame,config,1920,1080,diagnostics);
        Check(ImGui::GetBackgroundDrawList()->VtxBuffer.Size==boxes,"absent direction suppresses extra geometry");ImGui::Render();
        bool menu=true,restart=false;std::string status;
        ImGui::NewFrame();esp::RenderMenu(config,menu,"unused-config.json",status,"Test offsets",restart);ImGui::Render();
        ImGui::DestroyContext();std::cout<<"PASS: 10000-object culling, draw budget, category toggles and killer direction geometry\n";return 0;
    } catch(const std::exception& e) {if(ImGui::GetCurrentContext()) ImGui::DestroyContext();std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
