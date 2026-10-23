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
        Require(Near(Radians(180),pi),"degree to radian conversion");
        const auto forward=DirectionFromYaw(0),side=DirectionFromYaw(Radians(90));
        Require(Near(forward.x,0)&&Near(forward.z,1),"zero yaw faces +Z");
        Require(Near(side.x,1)&&Near(side.z,0),"positive yaw faces +X");
        for(float angle:{-720.0f,-90.0f,0.0f,60.0f,120.0f,360.0f})
            Require(Near(Length(DirectionFromYaw(Radians(angle))),1),"yaw direction normalized");
        for(float angle:{30.0f,90.0f,120.0f}) {
            const float half=Radians(angle)/2;
            const auto left=RotateYaw(forward,-half),rightEdge=RotateYaw(forward,half);
            Require(Near(Length(left),1)&&Near(Length(rightEdge),1),"cone boundary directions normalized");
            Require(Near(Dot(left,rightEdge),std::cos(Radians(angle))),"cone full angle matches boundary dot product");
            Require(Near(left.x,-rightEdge.x)&&Near(left.z,rightEdge.z),"cone symmetric about heading");
            Require(Near(Length((Vec3{2,0,3}+rightEdge*6)-Vec3{2,0,3}),6),"cone radius and arrow world length");
        }
        const auto narrow=WorldToScreen({5,0,10},Perspective(Radians(60),2,1,100),1000,500);
        const auto wide=WorldToScreen({5,0,10},Perspective(Radians(120),2,1,100),1000,500);
        Require(narrow&&wide&&narrow->x>wide->x&&wide->x>500,"wider FOV reduces screen displacement");
        for(float fov:{60.0f,90.0f,120.0f}) {
            const auto projection=Perspective(Radians(fov),2,1,100);
            const auto edge=WorldToScreen({0,10*std::tan(Radians(fov)/2),10},projection,1000,500);
            Require(edge&&Near(edge->y,0),"vertical FOV boundary maps to viewport top");
        }
        DemoProvider demo;const auto frame=demo.SampleAt(16.0f/9.0f,CameraSettings{},0);
        Require(frame.source.find("DEMO")!=std::string::npos,"demo source labeled");
        Require(frame.entities.size()==11,"demo entity count");
        std::set<EntityKind> kinds;std::set<std::uint64_t> ids;
        for(const auto& e:frame.entities) {
            kinds.insert(e.kind);ids.insert(e.id);
            Require(WorldToScreen(e.position,frame.camera.viewProjection,1920,1080).has_value(),"demo entity projects");
            if(e.kind==EntityKind::Survivor||e.kind==EntityKind::Killer)
                Require(e.top.y>e.position.y,"character top above feet");
            Require(e.lookDirection.has_value()==(e.kind==EntityKind::Killer),"only demo killer has optional direction");
        }
        Require(kinds.size()==5&&ids.size()==frame.entities.size(),"all categories and unique IDs");
        const auto before=demo.SampleAt(16.0f/9.0f,CameraSettings{},2);
        const auto after=demo.SampleAt(16.0f/9.0f,CameraSettings{},2.01f);
        const auto a=*before.entities.at(4).lookDirection,b=*after.entities.at(4).lookDirection;
        Require(Near(Length(a),1)&&Near(Length(b),1)&&Dot(a,b)>0.9999f&&Length(a-b)>0,"smooth nonzero killer yaw change");
        const auto wideFrame=demo.SampleAt(16.0f/9.0f,CameraSettings{120},0);
        Require(wideFrame.camera.viewProjection.m!=frame.camera.viewProjection.m,"camera FOV reaches provider matrix");
        Require(Near(wideFrame.camera.position.x,frame.camera.position.x),"FOV does not move camera");
        for(float invalid:{59.0f,121.0f,std::numeric_limits<float>::quiet_NaN()}) {
            bool threw=false;
            try {demo.SampleAt(1,CameraSettings{invalid},0);} catch(const std::invalid_argument&) {threw=true;}
            Require(threw,"invalid camera FOV rejected by demo provider");
        }
        std::cout<<"PASS: projection, clipping, FOV bounds, cone angles, normalized directions, smooth demo yaw and all five categories\n";
        return EXIT_SUCCESS;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}
}
