#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "../tests/support/fixture_contract.hpp"
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
extern "C" {
__declspec(dllexport) fixture::State* EspFixtureState=nullptr;
__declspec(dllexport) void* EspFixtureProbe=nullptr;
}
int main() {
    auto* state=static_cast<fixture::State*>(VirtualAlloc(nullptr,sizeof(fixture::State),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    SYSTEM_INFO si{};GetSystemInfo(&si);auto* probe=static_cast<char*>(VirtualAlloc(nullptr,si.dwPageSize*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!state || !probe)return 2;DWORD old{};if(!VirtualProtect(probe+si.dwPageSize,si.dwPageSize,PAGE_NOACCESS,&old))return 3;
    EspFixtureProbe=probe;EspFixtureState=state;
    state->magic=fixture::Magic;state->version=1;state->capacity=fixture::Capacity;state->headerSize=offsetof(fixture::State,entities);state->camera={0,2,-10};
    state->count=5;
    for(unsigned i=0;i<5;++i){auto& e=state->entities[i];e.id=i+1;e.kind=i;e.flags=1;e.position={static_cast<float>(i)*3-6,0,20};e.top=e.position+esp::Vec3{0,2,0};e.direction={0,0,1};std::strcpy(e.name,"Fixture entity");}
    bool running=true,paused=false,bad=false;float yaw=0;std::string commands;HANDLE input=GetStdHandle(STD_INPUT_HANDLE);
    std::cout<<"Controlled fixture PID "<<GetCurrentProcessId()<<"; commands: move camera remove bad restore pause resume quit\n"<<std::flush;
    while(running) {
        DWORD available=0;if(GetFileType(input)==FILE_TYPE_PIPE && PeekNamedPipe(input,nullptr,0,nullptr,&available,nullptr) && available){char data[1024];DWORD got=0;if(ReadFile(input,data,(std::min)(available,DWORD(sizeof data)),&got,nullptr))commands.append(data,got);}
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&state->revision));
        for(auto newline=commands.find('\n');newline!=std::string::npos;newline=commands.find('\n')){
            auto c=commands.substr(0,newline);commands.erase(0,newline+1);if(!c.empty()&&c.back()=='\r')c.pop_back();
            if(c=="quit")running=false;else if(c=="pause")paused=true;else if(c=="resume")paused=false;
            else if(c=="move"){state->entities[0].position.x+=5;state->entities[0].top.x+=5;}
            else if(c=="camera")yaw+=0.2f;
            else if(c=="remove" && state->count){for(unsigned i=1;i<state->count;++i)state->entities[i-1]=state->entities[i];--state->count;}
            else if(c=="bad")bad=true;else if(c=="restore")bad=false;
            else if(c=="bad-count")state->count=fixture::Capacity+1;else if(c=="empty")state->count=0;
        }
        if(!paused){++state->sequence;state->timestamp=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());}
        state->matrix=esp::Multiply(esp::Perspective(esp::Radians(90),16.0f/9.0f,0.1f,1000),esp::LookAt(state->camera,state->camera+esp::DirectionFromYaw(yaw)));
        if(bad)state->matrix.m[0]=std::numeric_limits<float>::quiet_NaN();
        if(state->count<=fixture::Capacity)for(unsigned i=0;i<state->count;++i)if(state->entities[i].kind==1)state->entities[i].direction=esp::DirectionFromYaw(static_cast<float>(GetTickCount64()%100000)*0.001f);
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&state->revision));Sleep(20);
    }
    EspFixtureState=nullptr;VirtualFree(state,0,MEM_RELEASE);VirtualFree(probe,0,MEM_RELEASE);return 0;
}
