#include "esp/renderer.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace esp {
static void Label(ImDrawList* draw,ImVec2 position,ImU32 color,std::string_view text) {
    const auto size=ImGui::CalcTextSize(text.data(),text.data()+text.size());position.x-=size.x/2;
    draw->AddText({position.x+1,position.y+1},IM_COL32(0,0,0,230),text.data(),text.data()+text.size());
    draw->AddText(position,color,text.data(),text.data()+text.size());
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
RenderStats RenderEntities(const Frame& frame,const VisualConfig& c,float width,float height,const RuntimeDiagnostics& diagnostics) {
    struct Candidate {const Entity* entity;Vec2 base;float distanceSq,x{},y{},half{},height{};};
    // Scratch capacity is reused; Frame itself is shared immutably and never copied here.
    thread_local std::vector<Candidate> candidates;
    candidates.clear();
    const float maxSq=c.maxDistance*c.maxDistance;
    if(c.enabled) for(const auto& e:frame.entities) {
        const bool show=(e.kind==EntityKind::Survivor&&c.survivors)||(e.kind==EntityKind::Killer&&c.killer)||
            (e.kind==EntityKind::Generator&&c.generators)||(e.kind==EntityKind::Hook&&c.hooks)||(e.kind==EntityKind::ExitGate&&c.exitGates);
        if(!show) continue;
        const Vec3 delta=e.position-frame.camera.position;const float distanceSq=Dot(delta,delta);
        if(!std::isfinite(distanceSq)||distanceSq>maxSq) continue;
        const auto base=WorldToScreen(e.position,frame.camera.viewProjection,width,height);if(!base) continue;
        Candidate item{&e,*base,distanceSq};
        if(e.kind==EntityKind::Survivor||e.kind==EntityKind::Killer) {
            const auto top=WorldToScreen(e.top,frame.camera.viewProjection,width,height);if(!top) continue;
            item.height=std::abs(base->y-top->y);if(item.height<2) continue;
            item.x=(base->x+top->x)/2;item.y=std::min(base->y,top->y);item.half=item.height*0.24f;
            if(item.x+item.half<0||item.x-item.half>width||item.y+item.height<0||item.y>height) continue;
        } else if(base->x<0||base->x>width||base->y<0||base->y>height) continue;
        candidates.push_back(item);
    }
    const auto visible=candidates.size();
    const auto nearest=[](const Candidate& a,const Candidate& b) {
        return a.distanceSq==b.distanceSq?a.entity->id<b.entity->id:a.distanceSq<b.distanceSq;
    };
    const auto budget=static_cast<std::size_t>(c.maxDrawEntities);
    if(candidates.size()>budget) {
        std::nth_element(candidates.begin(),candidates.begin()+budget,candidates.end(),nearest);candidates.resize(budget);
    }
    std::sort(candidates.begin(),candidates.end(),nearest);
    auto* draw=ImGui::GetBackgroundDrawList();draw->PushClipRect({0,0},{width,height},true);
    for(const auto& item:candidates) {
        const auto& e=*item.entity;ImU32 color=IM_COL32_WHITE;const char* symbol="?";
        switch(e.kind) {
        case EntityKind::Survivor:color=ImGui::ColorConvertFloat4ToU32({c.survivorColor[0],c.survivorColor[1],c.survivorColor[2],c.survivorColor[3]});break;
        case EntityKind::Killer:color=ImGui::ColorConvertFloat4ToU32({c.killerColor[0],c.killerColor[1],c.killerColor[2],c.killerColor[3]});break;
        case EntityKind::Generator:color=IM_COL32(255,210,70,255);symbol="G";break;
        case EntityKind::Hook:color=IM_COL32(220,140,255,255);symbol="H";break;
        case EntityKind::ExitGate:color=IM_COL32(110,255,130,255);symbol="E";break;
        }
        if(e.kind==EntityKind::Killer) DrawKillerView(draw,e,frame,c,width,height);
        const float distance=std::sqrt(item.distanceSq);float labelY=item.base.y;
        if(e.kind==EntityKind::Survivor||e.kind==EntityKind::Killer) {
            const float x=item.x,y=item.y,half=item.half,h=item.height;
            draw->AddRect({x-half-1,y-1},{x+half+1,y+h+1},IM_COL32(0,0,0,220),0,0,3);
            draw->AddRect({x-half,y},{x+half,y+h},color,0,0,1.5f);
            if(c.names) Label(draw,{x,y-18},color,e.name);
            if(c.distances) {char text[40];std::snprintf(text,sizeof(text),"%.0f m",distance);Label(draw,{x,y+h+3},color,text);}
        } else {
            draw->AddCircleFilled({item.base.x,item.base.y},12,IM_COL32(15,18,25,220));
            draw->AddCircle({item.base.x,item.base.y},12,color,0,2);Label(draw,{item.base.x,item.base.y-7},color,symbol);
            labelY+=16;
            if(c.names) {Label(draw,{item.base.x,labelY},color,e.name);labelY+=16;}
            if(c.distances) {char text[40];std::snprintf(text,sizeof(text),"%.0f m",distance);Label(draw,{item.base.x,labelY},color,text);}
        }
    }
    draw->PopClipRect();
    ImGui::SetNextWindowPos({12,12},ImGuiCond_Always);ImGui::SetNextWindowBgAlpha(0.8f);
    if(ImGui::Begin("Source diagnostics",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::TextColored({1,0.8f,0.2f,1},"%s | %s%s",SourceKindName(diagnostics.kind),SourceStateName(diagnostics.state),diagnostics.fallback?" | DEMO FALLBACK":"");
        ImGui::Text("DBD %.*s: %.*s (no verified game adapter)",static_cast<int>(diagnostics.dbd.targetVersion.size()),diagnostics.dbd.targetVersion.data(),
            static_cast<int>(diagnostics.dbd.state.size()),diagnostics.dbd.state.data());
        ImGui::TextUnformatted(frame.source.c_str());
        ImGui::Text("Render %.1f FPS | Source %.1f FPS | Acquire %.2f ms | Age %.0f ms",ImGui::GetIO().Framerate,diagnostics.sourceFps,diagnostics.acquisitionMs,diagnostics.ageMs+std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-diagnostics.measuredAt).count());
        ImGui::Text("Objects %zu | Visible %zu | Drawn %zu | Accepted %llu | Invalid polls %llu",frame.entities.size(),visible,candidates.size(),
            static_cast<unsigned long long>(diagnostics.acceptedFrames),static_cast<unsigned long long>(diagnostics.invalidFrames));
        if(!diagnostics.message.empty()) ImGui::TextWrapped("%s",diagnostics.message.c_str());
        ImGui::Text("Record: %s",diagnostics.recording.c_str());
        ImGui::Text("INSERT menu | F1 all: %s | END quit",c.enabled?"ON":"OFF");
        ImGui::TextUnformatted("F2 survivors | F3 killer | F4 generators | F5 hooks | F6 gates | F7 names | F8 distance");
    } ImGui::End();
    return {frame.entities.size(),visible,candidates.size()};
}
static void EditPath(const char* label,std::string& value) {
    std::array<char,1024> buffer{};
    std::copy_n(value.data(),std::min(value.size(),buffer.size()-1),buffer.data());
    if(ImGui::InputText(label,buffer.data(),buffer.size())) value=buffer.data();
}
void RenderMenu(VisualConfig& c,bool& open,const std::filesystem::path& path,std::string& status,const std::string& offsetsStatus,bool& restartSource) {
    ImGui::SetNextWindowSize({510,460},ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Visual ESP | DEMO",&open)) {
        if(ImGui::BeginTabBar("Tabs")) {
            if(ImGui::BeginTabItem("VISUALS")) {
                ImGui::Checkbox("Enabled [F1]",&c.enabled);
                ImGui::Checkbox("Survivors [F2]",&c.survivors);ImGui::SameLine();ImGui::Checkbox("Killer [F3]",&c.killer);
                ImGui::Checkbox("Generators [F4]",&c.generators);ImGui::SameLine();ImGui::Checkbox("Hooks [F5]",&c.hooks);
                ImGui::Checkbox("Exit Gates [F6]",&c.exitGates);
                ImGui::Checkbox("Names [F7]",&c.names);ImGui::SameLine();ImGui::Checkbox("Distances [F8]",&c.distances);
                ImGui::SliderFloat("Max distance (m)",&c.maxDistance,1,1000,"%.0f",ImGuiSliderFlags_AlwaysClamp);
                ImGui::SliderInt("Draw budget",&c.maxDrawEntities,1,static_cast<int>(MaxFrameEntities),"%d",ImGuiSliderFlags_AlwaysClamp);
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
                ImGui::TextWrapped("Sources: demo, local JSON or recording replay. No game process is accessed.");
                int mode=static_cast<int>(c.runtime.kind);
                if(ImGui::Combo("Source",&mode,"Demo\0Local JSON\0Replay\0Live memory (Windows x64)\0")) c.runtime.kind=static_cast<SourceKind>(mode);
                if(c.runtime.kind==SourceKind::JsonFile) EditPath("JSON frame file",c.runtime.jsonPath);
                if(c.runtime.kind==SourceKind::Replay) {EditPath("Replay file",c.runtime.replayPath);ImGui::Checkbox("Loop replay",&c.runtime.replayLoop);}
                if(c.runtime.kind==SourceKind::LiveMemory) {EditPath("Verified memory layout",c.runtime.memoryLayoutPath);ImGui::TextUnformatted("Live memory never falls back to Demo. DBD connection unverified.");}
                else ImGui::Checkbox("Use labelled demo when source is lost",&c.runtime.fallbackDemo);
                ImGui::Checkbox("Record frames",&c.runtime.record);
                if(c.runtime.record) EditPath("Recording output",c.runtime.recordingPath);
                ImGui::SliderInt("Poll interval (ms)",&c.runtime.pollMs,5,1000,"%d",ImGuiSliderFlags_AlwaysClamp);
                ImGui::SliderInt("Stale timeout (ms)",&c.runtime.staleMs,100,30000,"%d",ImGuiSliderFlags_AlwaysClamp);
                if(ImGui::Button("Apply source / restart replay")) restartSource=true;
                ImGui::TextWrapped("Source and recording changes take effect with Apply. Camera FOV controls the demo camera; external frames supply their own matrix.");
                ImGui::Separator();
                if(ImGui::Button("Save")) SaveConfig(path,c,status);
                ImGui::SameLine();
                if(ImGui::Button("Load")) {if(LoadConfig(path,c,status)) restartSource=true;}
                ImGui::SameLine();
                if(ImGui::Button("Reset defaults")) {c=VisualConfig{};restartSource=true;status="Defaults restored (not saved)";}
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
