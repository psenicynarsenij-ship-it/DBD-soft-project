#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "esp/game_state_decoder.hpp"
#include "esp/runtime_provider.hpp"
#include "esp/overlay.hpp"
#include "esp/renderer.hpp"
#include "support/fixture_contract.hpp"
#include <fstream>
#include <iostream>
#include <functional>
namespace {
void Check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
struct Child {
    HANDLE process=nullptr,input=nullptr;DWORD pid{};
    explicit Child(const std::filesystem::path& executable){
        SECURITY_ATTRIBUTES sa{sizeof sa,nullptr,TRUE};HANDLE read=nullptr;
        Check(CreatePipe(&read,&input,&sa,0)!=0,"CreatePipe");SetHandleInformation(input,HANDLE_FLAG_INHERIT,0);
        STARTUPINFOW si{};si.cb=sizeof si;si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=read;si.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);si.hStdError=GetStdHandle(STD_ERROR_HANDLE);
        PROCESS_INFORMATION pi{};std::wstring command=L"\""+executable.wstring()+L"\"";
        BOOL ok=CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi);CloseHandle(read);
        if(!ok){CloseHandle(input);input=nullptr;throw std::runtime_error("CreateProcess fixture Win32="+std::to_string(GetLastError()));}
        process=pi.hProcess;pid=pi.dwProcessId;CloseHandle(pi.hThread);
    }
    void Send(const std::string& c){DWORD sent{};auto command=c+"\n";Check(WriteFile(input,command.data(),static_cast<DWORD>(command.size()),&sent,nullptr)&&sent==command.size(),"Send fixture command");}
    void Exit(){if(process){Send("quit");Check(WaitForSingleObject(process,3000)==WAIT_OBJECT_0,"Fixture clean exit");CloseHandle(process);process=nullptr;}}
    ~Child(){if(process){TerminateProcess(process,99);WaitForSingleObject(process,3000);CloseHandle(process);}if(input)CloseHandle(input);}
};
std::shared_ptr<const esp::RuntimeSnapshot> Wait(esp::RuntimeDataProvider& runtime,const std::function<bool(const esp::RuntimeSnapshot&)>& predicate){
    auto until=std::chrono::steady_clock::now()+std::chrono::seconds(6);std::shared_ptr<const esp::RuntimeSnapshot> snap;
    do{snap=runtime.Latest();if(predicate(*snap))return snap;Sleep(10);}while(std::chrono::steady_clock::now()<until);
    throw std::runtime_error("Runtime timeout: "+snap->diagnostics.message);
}
void Paint(esp::Overlay& overlay,const esp::RuntimeSnapshot& snapshot,const std::filesystem::path& capture){
    overlay.Poll();Check(overlay.BeginFrame(),"BeginFrame");esp::VisualConfig config;config.killerCone=true;
    auto stats=esp::RenderEntities(*snapshot.frame,config,overlay.Width(),overlay.Height(),snapshot.diagnostics);
    Check(stats.drawn==5,"Existing renderer draws all five fixture categories");overlay.EndFrame(capture);
    // Verify actual GPU pixels in the survivor's projected box, outside the HUD.
    auto feet=esp::WorldToScreen(snapshot.frame->entities[0].position,snapshot.frame->camera.viewProjection,overlay.Width(),overlay.Height());
    auto head=esp::WorldToScreen(snapshot.frame->entities[0].top,snapshot.frame->camera.viewProjection,overlay.Width(),overlay.Height());Check(feet&&head,"Fixture projection visible");
    std::ifstream in(capture,std::ios::binary);BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};in.read(reinterpret_cast<char*>(&file),sizeof file);in.read(reinterpret_cast<char*>(&info),sizeof info);
    std::vector<unsigned char> pixels(info.biSizeImage);in.seekg(file.bfOffBits);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size());Check(bool(in),"Read actual backbuffer capture");
    int colored=0;float half=(feet->y-head->y)*0.25f;
    for(int y=(std::max)(0,int(head->y)-3);y<(std::min)(int(-info.biHeight),int(feet->y)+3);++y)
      for(int x=(std::max)(0,int(head->x-half)-3);x<(std::min)(int(info.biWidth),int(head->x+half)+3);++x){auto i=(static_cast<std::size_t>(y)*info.biWidth+x)*4;if(pixels[i]>120 && pixels[i+1]>120 && pixels[i+2]<100 && pixels[i+3]>0)++colored;}
    Check(colored>10,"Survivor box pixels exist in actual DX11 output");
}
}
int main(int argc,char** argv){try{
    Check(argc>=3,"Expected fixture executable and layout");const bool graphics=argc>3;
    std::filesystem::path exe=std::filesystem::absolute(argv[1]),layoutPath=std::filesystem::absolute(argv[2]);
    std::ifstream input(layoutPath);auto json=nlohmann::json::parse(input);auto layout=esp::MemoryLayout::Parse(json);
    auto child=std::make_unique<Child>(exe);esp::ProcessReader reader;
    for(int retry=0;;++retry){try{reader.Attach(layout.executable,child->pid);break;}catch(...){if(retry==100)throw;Sleep(10);}}
    auto module=reader.Module(layout.module);Check(module.size==layout.imageSize && module.timestamp==layout.peTimestamp,"Verified module fingerprint");
    std::uint64_t root=0;for(int i=0;i<100&&!root;++i){root=reader.Value<std::uint64_t>(reader.Export(module,"EspFixtureState"));if(!root)Sleep(10);}Check(root!=0,"Real exported state address");
    auto direct=esp::GameStateDecoder(layout).Read(reader,root);Check(direct&&direct->entities.size()==5,"ReadProcessMemory decoded actual process objects");
    SYSTEM_INFO system{};GetSystemInfo(&system);auto probe=reader.Value<std::uint64_t>(reader.Export(module,"EspFixtureProbe"));char bytes[16];bool partial=false;
    try{reader.Read(probe+system.dwPageSize-8,bytes,sizeof bytes);}catch(const esp::MemoryReadError& e){partial=e.transferred==8 && e.requested==16 && e.code==ERROR_NOACCESS;}
    Check(partial,"Partial read reports transferred bytes and Win32 error");
    bool bounds=false;try{reader.Read(UINT64_MAX-3,bytes,sizeof bytes);}catch(const esp::MemoryReadError&){bounds=true;}Check(bounds,"Overflow address rejected");
    auto options=esp::RuntimeOptions{};options.kind=esp::SourceKind::LiveMemory;options.memoryLayoutPath=layoutPath.string();options.fallbackDemo=true;options.pollMs=5;options.staleMs=150;
    esp::RuntimeDataProvider runtime(esp::MakeFrameSource(options,layoutPath.parent_path()),options);
    auto initial=Wait(runtime,[](auto& s){return s.frame->entities.size()==5;});Check(!initial->diagnostics.fallback&&!initial->diagnostics.dbd.connected,"Live does not claim DBD or synthesize Demo");
    nlohmann::json report={{"fixture_pid",child->pid},{"module_base",module.base},{"root_address",root},{"initial_x",initial->frame->entities[0].position.x},{"read_api","ReadProcessMemory"},{"dbd_verified",false}};
    std::unique_ptr<esp::Overlay> overlay;if(graphics){overlay=std::make_unique<esp::Overlay>();overlay->Initialize(GetModuleHandleW(nullptr));Paint(*overlay,*initial,"memory-before.bmp");}
    float x=initial->frame->entities[0].position.x;child->Send("move");auto moved=Wait(runtime,[&](auto& s){return s.frame->entities.size()==5 && s.frame->entities[0].position.x==x+5;});
    report["moved_x"]=moved->frame->entities[0].position.x;
    if(graphics)Paint(*overlay,*moved,"memory-after-move.bmp");
    auto matrix=moved->frame->camera.viewProjection.m;child->Send("camera");auto turned=Wait(runtime,[&](auto& s){return s.frame->entities.size()==5 && s.frame->camera.viewProjection.m!=matrix;});
    if(graphics)Paint(*overlay,*turned,"memory-after-camera.bmp");
    child->Send("remove");Wait(runtime,[](auto& s){return s.frame->entities.size()==4 && s.frame->entities.front().id==2;});
    child->Send("bad");Wait(runtime,[](auto& s){return s.frame->entities.empty() && s.diagnostics.state==esp::SourceState::Invalid;});
    child->Send("restore");Wait(runtime,[](auto& s){return s.frame->entities.size()==4;});
    child->Send("pause");Wait(runtime,[](auto& s){return s.frame->entities.empty() && !s.diagnostics.fallback;});
    child->Send("resume");Wait(runtime,[](auto& s){return s.frame->entities.size()==4;});
    child->Send("bad-count");Wait(runtime,[](auto& s){return s.frame->entities.empty() && s.diagnostics.state==esp::SourceState::Invalid;});
    child->Send("empty");Wait(runtime,[](auto& s){return s.diagnostics.state==esp::SourceState::Live && s.frame->entities.empty();});
    child->Exit();Wait(runtime,[](auto& s){return s.frame->entities.empty() && s.diagnostics.state==esp::SourceState::Reconnecting;});Check(!reader.Alive(),"Exit recognized on original handle");
    child.reset();child=std::make_unique<Child>(exe);auto recovered=Wait(runtime,[](auto& s){return s.frame->entities.size()==5;});
    report["reconnected_pid"]=child->pid;report["checks"]={"real_cross_process_read","partial_read","bounds","module_identity","movement","camera","disappearance","invalid_matrix","stale_clear","invalid_count","exit","reconnect","no_demo"};report["dx11_pixels_verified"]=graphics;
    runtime.Stop();child->Exit();std::ofstream out(graphics?"windows-overlay-report.json":"windows-memory-report.json");out<<report.dump(2)<<'\n';
    std::cout<<report.dump(2)<<"\nPASS Windows cross-process pipeline"<<(graphics?" and actual DX11 pixels":"")<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<" Win32="<<GetLastError()<<'\n';return 1;}}
