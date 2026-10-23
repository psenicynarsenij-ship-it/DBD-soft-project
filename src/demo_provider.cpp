#include "esp/demo_provider.hpp"
#include <algorithm>
#include <stdexcept>

namespace esp {
Frame DemoProvider::Sample(float aspect, const CameraSettings& camera) {
    const float t=std::chrono::duration<float>(std::chrono::steady_clock::now()-start_).count();
    return SampleAt(aspect,camera,t);
}
Frame DemoProvider::SampleAt(float aspect, const CameraSettings& camera, float t) const {
    if(!std::isfinite(camera.verticalFovDegrees)||camera.verticalFovDegrees<CameraSettings::MinFovDegrees||
       camera.verticalFovDegrees>CameraSettings::MaxFovDegrees)
        throw std::invalid_argument("Camera FOV must be 60..120 degrees");
    if(!std::isfinite(aspect)||aspect<=0||!std::isfinite(t))
        throw std::invalid_argument("Demo aspect must be positive and time must be finite");
    Frame f;
    f.source="DEMO DATA - no game connection";
    f.camera.position={std::sin(t*0.16f)*1.5f,2.0f,-8.0f};
    f.camera.viewProjection=Multiply(Perspective(Radians(camera.verticalFovDegrees),aspect,0.1f,500.0f),
                                     LookAt(f.camera.position,{0,1,12}));
    auto add=[&](std::uint64_t id,EntityKind kind,const char* name,Vec3 p,float height=0) {
        f.entities.push_back({id,kind,name,p,p+Vec3{0,height,0},std::nullopt});
    };
    add(1,EntityKind::Survivor,"Demo Survivor A",{-4+std::sin(t*0.6f),0,9},1.75f);
    add(2,EntityKind::Survivor,"Demo Survivor B",{1,0,14+std::cos(t*0.5f)},1.8f);
    add(3,EntityKind::Survivor,"Demo Survivor C",{5,0,20},1.7f);
    add(4,EntityKind::Survivor,"Demo Survivor D",{-7,0,24},1.8f);
    add(5,EntityKind::Killer,"Demo Killer",{4+std::sin(t*0.4f)*2,0,11},2.1f);
    f.entities.back().lookDirection=DirectionFromYaw(t*0.35f);
    add(6,EntityKind::Generator,"Generator A",{-6,0.8f,13});
    add(7,EntityKind::Generator,"Generator B",{8,0.8f,23});
    add(8,EntityKind::Hook,"Hook A",{-8,2.5f,19});
    add(9,EntityKind::Hook,"Hook B",{6,2.5f,29});
    add(10,EntityKind::ExitGate,"Exit Gate A",{-11,2,34});
    add(11,EntityKind::ExitGate,"Exit Gate B",{12,2,37});
    return f;
}
}
