#include "esp/demo_provider.hpp"
#include <algorithm>

namespace esp {
Frame DemoProvider::Sample(float aspect) {
    const float t=std::chrono::duration<float>(std::chrono::steady_clock::now()-start_).count();
    Frame f;
    f.source="DEMO DATA - no game connection";
    f.camera.position={std::sin(t*0.16f)*1.5f,2.0f,-8.0f};
    f.camera.viewProjection=Multiply(Perspective(1.04719755f,std::max(aspect,0.1f),0.1f,500.0f),
                                     LookAt(f.camera.position,{0,1,12}));
    auto add=[&](std::uint64_t id,EntityKind kind,const char* name,Vec3 p,float height=0) {
        f.entities.push_back({id,kind,name,p,p+Vec3{0,height,0}});
    };
    add(1,EntityKind::Survivor,"Demo Survivor A",{-4+std::sin(t*0.6f),0,9},1.75f);
    add(2,EntityKind::Survivor,"Demo Survivor B",{1,0,14+std::cos(t*0.5f)},1.8f);
    add(3,EntityKind::Survivor,"Demo Survivor C",{5,0,20},1.7f);
    add(4,EntityKind::Survivor,"Demo Survivor D",{-7,0,24},1.8f);
    add(5,EntityKind::Killer,"Demo Killer",{4+std::sin(t*0.4f)*2,0,11},2.1f);
    add(6,EntityKind::Generator,"Generator A",{-6,0.8f,13});
    add(7,EntityKind::Generator,"Generator B",{8,0.8f,23});
    add(8,EntityKind::Hook,"Hook A",{-8,2.5f,19});
    add(9,EntityKind::Hook,"Hook B",{6,2.5f,29});
    add(10,EntityKind::ExitGate,"Exit Gate A",{-11,2,34});
    add(11,EntityKind::ExitGate,"Exit Gate B",{12,2,37});
    return f;
}
}
