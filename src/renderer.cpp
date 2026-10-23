#include "esp/renderer.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace esp {
static void Label(ImDrawList* draw,ImVec2 position,ImU32 color,const std::string& text) {
    const auto size=ImGui::CalcTextSize(text.c_str());position.x-=size.x/2;
    draw->AddText({position.x+1,position.y+1},IM_COL32(0,0,0,230),text.c_str());
    draw->AddText(position,color,text.c_str());
}
void RenderEntities(const Frame& frame,const VisualConfig& c,float width,float height) {
    auto* draw=ImGui::GetBackgroundDrawList();
    draw->PushClipRect({0,0},{width,height},true);
    if(c.enabled) for(const auto& e:frame.entities) {
        const bool character=e.kind==EntityKind::Survivor||e.kind==EntityKind::Killer;
        bool show=false;ImU32 color=IM_COL32_WHITE;const char* symbol="?";
        switch(e.kind) {
        case EntityKind::Survivor:show=c.survivors;color=ImGui::ColorConvertFloat4ToU32({c.survivorColor[0],c.survivorColor[1],c.survivorColor[2],c.survivorColor[3]});break;
        case EntityKind::Killer:show=c.killer;color=ImGui::ColorConvertFloat4ToU32({c.killerColor[0],c.killerColor[1],c.killerColor[2],c.killerColor[3]});break;
        case EntityKind::Generator:show=c.generators;color=IM_COL32(255,210,70,255);symbol="G";break;
        case EntityKind::Hook:show=c.hooks;color=IM_COL32(220,140,255,255);symbol="H";break;
        case EntityKind::ExitGate:show=c.exitGates;color=IM_COL32(110,255,130,255);symbol="E";break;
        }
        const float distance=Length(e.position-frame.camera.position);
        if(!show||!std::isfinite(distance)||distance>c.maxDistance) continue;
        const auto base=WorldToScreen(e.position,frame.camera.viewProjection,width,height);
        if(!base) continue;
        float labelY=base->y;
        if(character) {
            const auto top=WorldToScreen(e.top,frame.camera.viewProjection,width,height);if(!top) continue;
            const float h=std::abs(base->y-top->y);if(h<2) continue;
            const float center=(base->x+top->x)/2;
            const float y=std::min(base->y,top->y),half=h*0.24f;
            if(center+half<0||center-half>width||y+h<0||y>height) continue;
            draw->AddRect({center-half-1,y-1},{center+half+1,y+h+1},IM_COL32(0,0,0,220),0,0,3);
            draw->AddRect({center-half,y},{center+half,y+h},color,0,0,1.5f);
            if(c.names) Label(draw,{center,y-18},color,e.name);
            if(c.distances) {char s[40];std::snprintf(s,sizeof(s),"%.0f m",distance);Label(draw,{center,y+h+3},color,s);}
        } else {
            if(base->x<0||base->x>width||base->y<0||base->y>height) continue;
            draw->AddCircleFilled({base->x,base->y},12,IM_COL32(15,18,25,220));
            draw->AddCircle({base->x,base->y},12,color,0,2);
            Label(draw,{base->x,base->y-7},color,symbol);
            labelY+=16;
            if(c.names) {Label(draw,{base->x,labelY},color,e.name);labelY+=16;}
            if(c.distances) {char s[40];std::snprintf(s,sizeof(s),"%.0f m",distance);Label(draw,{base->x,labelY},color,s);}
        }
    }
    draw->PopClipRect();
    ImGui::SetNextWindowPos({12,12},ImGuiCond_Always);ImGui::SetNextWindowBgAlpha(0.8f);
    if(ImGui::Begin("Demo status",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::TextColored({1,0.8f,0.2f,1},"DEMO - NO GAME CONNECTION");
        ImGui::TextUnformatted(frame.source.c_str());
        ImGui::Text("INSERT menu | F1 all: %s | END quit",c.enabled?"ON":"OFF");
        ImGui::TextUnformatted("F2 survivors | F3 killer | F4 generators | F5 hooks | F6 gates");
    } ImGui::End();
}
void RenderMenu(VisualConfig& c,bool& open,const std::filesystem::path& path,std::string& status,const std::string& offsetsStatus) {
    ImGui::SetNextWindowSize({510,460},ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Visual ESP | DEMO",&open)) {
        if(ImGui::BeginTabBar("Tabs")) {
            if(ImGui::BeginTabItem("VISUALS")) {
                ImGui::Checkbox("Enabled [F1]",&c.enabled);
                ImGui::Checkbox("Survivors [F2]",&c.survivors);ImGui::SameLine();ImGui::Checkbox("Killer [F3]",&c.killer);
                ImGui::Checkbox("Generators [F4]",&c.generators);ImGui::SameLine();ImGui::Checkbox("Hooks [F5]",&c.hooks);
                ImGui::Checkbox("Exit Gates [F6]",&c.exitGates);
                ImGui::Checkbox("Names [F7]",&c.names);ImGui::SameLine();ImGui::Checkbox("Distances [F8]",&c.distances);
                ImGui::SliderFloat("Max distance (m)",&c.maxDistance,1,1000,"%.0f");
                ImGui::ColorEdit4("Survivor color",c.survivorColor.data());
                ImGui::ColorEdit4("Killer color",c.killerColor.data());
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("CONFIG")) {
                ImGui::TextWrapped("Provider: demo. Target version label: DBD 10.2.2. No process memory is accessed.");
                if(ImGui::Button("Save")) SaveConfig(path,c,status);
                ImGui::SameLine();
                if(ImGui::Button("Load")) LoadConfig(path,c,status);
                ImGui::SameLine();
                if(ImGui::Button("Reset defaults")) {c=VisualConfig{};status="Defaults restored (not saved)";}
                ImGui::TextWrapped("%s",status.c_str());
                ImGui::Separator();ImGui::TextWrapped("%s",offsetsStatus.c_str());
                ImGui::TextWrapped("Files: config.json and offsets.json beside the executable.");
                ImGui::TextWrapped("INSERT: show/hide menu. Hidden menu enables click-through; visible menu captures mouse input. END: exit.");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    } ImGui::End();
}
}
