#include "esp/frame_sources.hpp"
#include "esp/dbd_connection.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
namespace {
int Number(const char* text,int limit) {
    std::size_t used{};const auto value=std::stoi(text,&used);
    if(text[used]!='\0'||value<1||value>limit) throw std::runtime_error("numeric argument outside allowed range");return value;
}
esp::Frame Demo(std::uint64_t sequence,int count) {
    esp::DemoProvider provider;
    auto frame=provider.SampleAt(16.0f/9.0f,esp::CameraSettings{90},static_cast<float>(sequence)*0.016f);
    if(count<static_cast<int>(frame.entities.size())) frame.entities.resize(static_cast<std::size_t>(count));
    while(frame.entities.size()<static_cast<std::size_t>(count)) {
        const auto id=frame.entities.size()+1;
        esp::Vec3 position{static_cast<float>(id%100)-50,0.8f,10+static_cast<float>(id/100)};
        frame.entities.push_back({id,esp::EntityKind::Generator,"Synthetic generator",position,position,std::nullopt});
    }
    frame.sequence=sequence;frame.timestampUs=sequence*16000;frame.source="SYNTHETIC FILE / NOT GAME DATA";return frame;
}

}
int main(int argc,char** argv) {
    try {
        if(argc==2&&std::string(argv[1])=="dbd-status") {
            std::cout<<esp::DbdBuildStatusJson()<<'\n';return 3; // Expected: no verified game adapter.
        }
        if(argc<3) {std::cerr<<"frame_tool generate FILE [objects=11]\nframe_tool publish FILE [frames=300] [objects=11]\nframe_tool record FILE [frames=120] [objects=11]\nframe_tool validate FILE\nframe_tool benchmark OBJECTS [iterations=10]\nframe_tool dbd-status (exit 3: not connected)\n";return 2;}
        const std::string command=argv[1];const auto path=esp::ResolveDataPath(std::filesystem::current_path(),argv[2]);
        if(command=="generate") {
            const int count=argc>3?Number(argv[3],static_cast<int>(esp::MaxFrameEntities)):11;
            esp::AtomicWriteText(path,esp::SerializeFrame(Demo(1,count)));std::cout<<"Generated synthetic snapshot: "<<count<<" objects\n";
        } else if(command=="publish"||command=="record") {
            const int frames=argc>3?Number(argv[3],100000):command=="publish"?300:120;
            const int count=argc>4?Number(argv[4],static_cast<int>(esp::MaxFrameEntities)):11;
            std::unique_ptr<esp::FrameRecorder> recorder;
            if(command=="record") recorder=std::make_unique<esp::FrameRecorder>(path);
            const auto started=std::chrono::steady_clock::now();
            for(int i=1;i<=frames;++i) {
                const auto frame=Demo(static_cast<std::uint64_t>(i),count);
                if(recorder) recorder->Write(frame,std::chrono::steady_clock::now());else esp::AtomicWriteText(path,esp::SerializeFrame(frame));
                std::this_thread::sleep_until(started+std::chrono::milliseconds(i*16));
            }
            std::cout<<frames<<" synthetic frames written\n";
        } else if(command=="validate") {
            if(path.extension()==".jsonl") {
                esp::ReplaySource replay(path,false);esp::ReadContext context;std::size_t count{};
                for(;;) {
                    const auto result=replay.Poll(context);
                    if(result.state==esp::ReadState::Ended) break;
                    if(result.state==esp::ReadState::Invalid||result.state==esp::ReadState::Disconnected) throw std::runtime_error(result.message);
                    if(result.state==esp::ReadState::Ready) ++count;
                    context.now+=std::chrono::hours(1);
                }
                if(count==0) throw std::runtime_error("recording contains no frames");
                std::cout<<"Valid recording: "<<count<<" frames\n";
            } else {
                esp::JsonFileSource source(path);const auto result=source.Poll(esp::ReadContext{});
                if(result.state!=esp::ReadState::Ready) throw std::runtime_error(result.message);
                std::cout<<"Valid snapshot: "<<result.frame->entities.size()<<" objects\n";
            }
        } else if(command=="benchmark") {
            const int count=Number(argv[2],static_cast<int>(esp::MaxFrameEntities));const int iterations=argc>3?Number(argv[3],1000):10;
            const auto frame=Demo(1,count);const auto start=std::chrono::steady_clock::now();std::size_t bytes{};
            for(int i=0;i<iterations;++i) {const auto encoded=esp::SerializeFrame(frame);const auto decoded=esp::DeserializeFrame(encoded);if(decoded.entities.size()!=frame.entities.size()) throw std::runtime_error("round trip mismatch");bytes=encoded.size();}
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/iterations;
            std::cout<<"Objects="<<count<<" bytes="<<bytes<<" encode+decode+validation mean_ms="<<ms<<" iterations="<<iterations<<'\n';
        } else throw std::runtime_error("unknown command");
        return 0;
    } catch(const std::exception& e) {std::cerr<<"Error: "<<e.what()<<'\n';return 1;}
}
