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
static void DrawKillerView(ImDrawList* draw,const Entity& e,const Frame& frame,const VisualConfig& c,float width,float height) {
    if(!e.lookDirection) return;
    const float norm=Length(*e.lookDirection);
    if(!std::isfinite(norm)||std::abs(norm-1)>0.02f) return;
    const Vec3 direction=Normalize(*e.lookDirection);
    auto project=[&](Vec3 p) {return WorldToScreen(p,frame.camera.viewProjection,width,height);};
    auto tint=[&](float alpha) {return ImGui::ColorConvertFloat4ToU32({c.killerArrowColor[0],c.killerArrowColor[1],c.killerArrowColor[2],alpha});};
    if(c.killerCone) {
        // A horizontal ground-plane view sector; its radius matches the arrow length.
        const Vec3 horizontal{direction.x,0,direction.z};
        if(Length(horizontal)>0.0001f) {
            const Vec3 forward=Normalize(horizontal),origin=e.position+Vec3{0,0.08f,0};
            const auto center=project(origin);
            constexpr int segments=32;
            const float half=Radians(c.killerConeDegrees)*0.5f;
            auto previous=project(origin+RotateYaw(forward,-half)*c.killerArrowLength);
            if(center&&previous) draw->AddLine({center->x,center->y},{previous->x,previous->y},tint(c.killerArrowOpacity));
            for(int i=1;i<=segments;++i) {
                const float angle=-half+2*half*static_cast<float>(i)/segments;
                const auto next=project(origin+RotateYaw(forward,angle)*c.killerArrowLength);
                if(center&&previous&&next) {
                    draw->AddTriangleFilled({center->x,center->y},{previous->x,previous->y},{next->x,next->y},tint(c.killerConeOpacity));
                    draw->AddLine({previous->x,previous->y},{next->x,next->y},tint(c.killerArrowOpacity));
                }
                previous=next;
            }
            if(center&&previous) draw->AddLine({center->x,center->y},{previous->x,previous->y},tint(c.killerArrowOpacity));
        }
    }
    if(c.killerDirection) {
        const auto start=project(e.top),end=project(e.top+direction*c.killerArrowLength);
        if(!start||!end) return;
        const float dx=end->x-start->x,dy=end->y-start->y,len=std::hypot(dx,dy);
        if(len<2) return; // Nearly end-on arrows have no stable screen-space heading.
        const float ux=dx/len,uy=dy/len,head=std::min(14.0f,len*0.3f);
        const ImVec2 tip{end->x,end->y};
        const auto color=tint(c.killerArrowOpacity);
        draw->AddLine({start->x,start->y},tip,color,2.5f);
        draw->AddTriangleFilled(tip,{end->x-ux*head-uy*head*0.45f,end->y-uy*head+ux*head*0.45f},
                                   {end->x-ux*head+uy*head*0.45f,end->y-uy*head-ux*head*0.45f},color);
    }
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
        if(e.kind==EntityKind::Killer) DrawKillerView(draw,e,frame,c,width,height);
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
                ImGui::SeparatorText("Demo camera");
                ImGui::SliderFloat("Camera vertical FOV",&c.camera.verticalFovDegrees,CameraSettings::MinFovDegrees,CameraSettings::MaxFovDegrees,"%.0f deg",ImGuiSliderFlags_AlwaysClamp);
                ImGui::SeparatorText("Demo killer view");
                ImGui::Checkbox("Look direction arrow",&c.killerDirection);
                ImGui::Checkbox("View cone (ground sector)",&c.killerCone);
                ImGui::SliderFloat("Cone angle",&c.killerConeDegrees,30,120,"%.0f deg",ImGuiSliderFlags_AlwaysClamp);
                ImGui::SliderFloat("Arrow / cone length",&c.killerArrowLength,1,20,"%.1f m",ImGuiSliderFlags_AlwaysClamp);
                ImGui::ColorEdit3("Direction color",c.killerArrowColor.data());
                ImGui::SliderFloat("Arrow / outline opacity",&c.killerArrowOpacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                ImGui::SliderFloat("Cone fill opacity",&c.killerConeOpacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
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
