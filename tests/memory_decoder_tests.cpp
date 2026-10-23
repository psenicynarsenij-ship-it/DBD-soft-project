#include "esp/game_state_decoder.hpp"
#include "esp/runtime_provider.hpp"
#include "support/fixture_contract.hpp"
#include <cstring>
#include <iostream>
#include <thread>
namespace {
void Check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
template<class F> void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected,"Expected rejection");}
struct Reader:esp::IMemoryReader {
    fixture::State state{};mutable bool tear=false;
    void Read(std::uint64_t a,void* out,std::size_t n)const override{
        if(a<0x10000 || a-0x10000>sizeof state || n>sizeof state-(a-0x10000))throw esp::MemoryReadError("Test bounds",a,n,0,487);
        std::memcpy(out,reinterpret_cast<const char*>(&state)+(a-0x10000),n);
        if(tear && n==8 && a==0x10000+offsetof(fixture::State,revision)){auto changed=state.revision+2;std::memcpy(out,&changed,8);}
    }
};
}
int main(){try{
    auto json=fixture::Layout(4096,123);auto layout=esp::MemoryLayout::Parse(json);esp::GameStateDecoder decoder(layout);Reader reader;
    auto& s=reader.state;s.magic=fixture::Magic;s.version=1;s.count=1;s.sequence=7;s.timestamp=123;s.revision=2;
    s.camera={0,2,-10};s.matrix=esp::Multiply(esp::Perspective(esp::Radians(90),16.0f/9,0.1f,1000),esp::LookAt(s.camera,{0,2,10}));
    auto& e=s.entities[0];e.id=1;e.kind=1;e.flags=1;e.position={0,0,10};e.top={0,2,10};e.direction={0,0,1};std::strcpy(e.name,"Memory object");
    auto f=decoder.Read(reader,0x10000);Check(f && f->sequence==7 && f->entities.size()==1 && f->entities[0].lookDirection.has_value(),"Decode complete memory snapshot");
    e.flags=0;Check(!decoder.Read(reader,0x10000)->entities[0].lookDirection,"Optional direction");e.flags=1;
    s.revision=3;Check(!decoder.Read(reader,0x10000),"Odd revision deferred");s.revision=2;reader.tear=true;Check(!decoder.Read(reader,0x10000),"Torn snapshot discarded");reader.tear=false;
    s.count=65;Reject([&]{decoder.Read(reader,0x10000);});s.count=1;
    e.id=0;Reject([&]{decoder.Read(reader,0x10000);});e.id=1;
    e.direction={0,0,4};Reject([&]{decoder.Read(reader,0x10000);});e.direction={0,0,1};
    e.position.x=std::numeric_limits<float>::quiet_NaN();Reject([&]{decoder.Read(reader,0x10000);});e.position.x=0;
    e.kind=99;Reject([&]{decoder.Read(reader,0x10000);});e.kind=1;
    std::memset(e.name,'x',sizeof e.name);Reject([&]{decoder.Read(reader,0x10000);});e.name[0]=0;
    s.count=0;Check(decoder.Read(reader,0x10000)->entities.empty(),"Disappearance is authoritative");
    auto bad=json;bad["entity"]["stride"]=4;Reject([&]{esp::MemoryLayout::Parse(bad);});
    bad=json;bad["max_entities"]=-1;Reject([&]{esp::MemoryLayout::Parse(bad);});
    bad=json;bad["pointer_chain"]=std::vector<int>(9,0);Reject([&]{esp::MemoryLayout::Parse(bad);});
    bad=json;bad["header"]["view_projection"]=120;Reject([&]{esp::MemoryLayout::Parse(bad);});
    bad=json;bad["root_rva"]="0x100";Reject([&]{esp::MemoryLayout::Parse(bad);});
    bad=json;bad["entity"]["kinds"]={0,0,2,3,4};Reject([&]{esp::MemoryLayout::Parse(bad);});
    bad=json;bad["coordinate_contract"]="guessed";Reject([&]{esp::MemoryLayout::Parse(bad);});
    esp::RuntimeOptions options;options.kind=esp::SourceKind::LiveMemory;options.fallbackDemo=true;options.pollMs=5;options.staleMs=100;
    esp::RuntimeDataProvider runtime(std::make_unique<esp::LiveFrameSource>("missing-layout-for-test.json"),options);
    std::this_thread::sleep_for(std::chrono::milliseconds(40));auto snap=runtime.Latest();Check(snap->frame->entities.empty() && !snap->diagnostics.fallback,"Live mode never substitutes Demo");runtime.Stop();
    std::cout<<"PASS memory decoding, coherence, bounds, validation, disappearance, no Demo fallback\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
