#include "esp/demo_provider.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

namespace {
void Require(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}
bool Near(float a,float b) {return std::abs(a-b)<0.01f;}
}
int main() {
    try {
        using namespace esp;
        constexpr float pi=3.14159265359f;
        const auto p=Perspective(pi/2,2,1,100);
        const auto center=WorldToScreen({0,0,10},p,1000,500);
        Require(center&&Near(center->x,500)&&Near(center->y,250),"center projection");
        const auto right=WorldToScreen({10,0,10},p,1000,500);
        Require(right&&Near(right->x,750),"aspect ratio correction");
        const auto up=WorldToScreen({0,5,10},p,1000,500);
        Require(up&&Near(up->y,125),"Y inversion");
        Require(!WorldToScreen({0,0,-5},p,1000,500),"behind camera rejected");
        Require(!WorldToScreen({0,0,0},p,1000,500),"zero W rejected");
        Require(!WorldToScreen({0,0,0.5f},p,1000,500),"near clipping");
        Require(!WorldToScreen({0,0,101},p,1000,500),"far clipping");
        Require(!WorldToScreen({0,0,10},p,0,500),"invalid viewport rejected");
        Require(!WorldToScreen({std::numeric_limits<float>::quiet_NaN(),0,10},p,1000,500),"NaN rejected");
        const Vec3 eye{4,2,-3};
        const auto vp=Multiply(p,LookAt(eye,eye+Vec3{1,0,0}));
        const auto turned=WorldToScreen(eye+Vec3{10,0,0},vp,1000,500);
        Require(turned&&Near(turned->x,500)&&Near(turned->y,250),"translated rotated camera");
        Require(Near(Length({3,0,4}),5),"meter distance");
        DemoProvider demo;const auto frame=demo.Sample(16.0f/9.0f);
        Require(frame.source.find("DEMO")!=std::string::npos,"demo source labeled");
        Require(frame.entities.size()==11,"demo entity count");
        std::set<EntityKind> kinds;std::set<std::uint64_t> ids;
        for(const auto& e:frame.entities) {
            kinds.insert(e.kind);ids.insert(e.id);
            Require(WorldToScreen(e.position,frame.camera.viewProjection,1920,1080).has_value(),"demo entity projects");
            if(e.kind==EntityKind::Survivor||e.kind==EntityKind::Killer)
                Require(e.top.y>e.position.y,"character top above feet");
        }
        Require(kinds.size()==5&&ids.size()==frame.entities.size(),"all categories and unique IDs");
        std::cout<<"PASS: projection, clipping, camera transforms, distances, demo snapshot and all five categories\n";
        return EXIT_SUCCESS;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}
}
