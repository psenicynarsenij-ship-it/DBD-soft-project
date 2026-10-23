#include "esp/frame_io.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace esp {
using nlohmann::json;
namespace {
void Require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
bool ValidText(const std::string& text,std::size_t limit) {
    if(text.size()>limit) return false;
    for(std::size_t i=0;i<text.size();) {
        const auto c=static_cast<unsigned char>(text[i++]);
        if(c<0x20||c==0x7f) return false;
        if(c<0x80) continue;
        unsigned value{},count{},minimum{};
        if(c>=0xc2&&c<=0xdf) {value=c&31;count=1;minimum=0x80;}
        else if(c>=0xe0&&c<=0xef) {value=c&15;count=2;minimum=0x800;}
        else if(c>=0xf0&&c<=0xf4) {value=c&7;count=3;minimum=0x10000;}
        else return false;
        if(i+count>text.size()) return false;
        while(count--) {const auto next=static_cast<unsigned char>(text[i++]);if((next&0xc0)!=0x80) return false;value=(value<<6)|(next&63);}
        if(value<minimum||value>0x10ffff||(value>=0xd800&&value<=0xdfff)) return false;
    }
    return true;
}
bool FinitePosition(Vec3 v) {
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&
           std::abs(v.x)<=1e6f&&std::abs(v.y)<=1e6f&&std::abs(v.z)<=1e6f;
}
bool ValidMatrix(const Matrix4& matrix) {
    double a[4][4]{};
    for(int y=0;y<4;++y) for(int x=0;x<4;++x) {
        const float v=matrix.m[y*4+x];if(!std::isfinite(v)||std::abs(v)>1e6f) return false;a[y][x]=v;
    }
    // Partial-pivot elimination accepts perspective and orthographic matrices.
    for(int c=0;c<4;++c) {
        int pivot=c;for(int r=c+1;r<4;++r) if(std::abs(a[r][c])>std::abs(a[pivot][c])) pivot=r;
        if(std::abs(a[pivot][c])<1e-10) return false;
        for(int k=0;k<4;++k) std::swap(a[c][k],a[pivot][k]);
        for(int r=c+1;r<4;++r) {const double ratio=a[r][c]/a[c][c];for(int k=c;k<4;++k) a[r][k]-=ratio*a[c][k];}
    }
    return true;
}
const char* Kind(EntityKind kind) {
    switch(kind) {
    case EntityKind::Survivor:return "survivor";
    case EntityKind::Killer:return "killer";
    case EntityKind::Generator:return "generator";
    case EntityKind::Hook:return "hook";
    case EntityKind::ExitGate:return "exit_gate";
    }
    throw std::runtime_error("unknown entity kind");
}
EntityKind ParseKind(const std::string& kind) {
    for(auto k:{EntityKind::Survivor,EntityKind::Killer,EntityKind::Generator,EntityKind::Hook,EntityKind::ExitGate}) if(kind==Kind(k)) return k;
    throw std::runtime_error("unknown entity kind");
}
std::uint64_t Unsigned(const json& j) {
    Require(j.is_number_unsigned()||(j.is_number_integer()&&j.get<std::int64_t>()>=0),"expected unsigned integer");
    return j.get<std::uint64_t>();
}
Vec3 Vector(const json& j) {
    Require(j.is_array()&&j.size()==3,"expected a vector with 3 numeric components");
    for(const auto& v:j) Require(v.is_number(),"vector component must be numeric");
    return {j[0].get<float>(),j[1].get<float>(),j[2].get<float>()};
}
json Vector(Vec3 v) {return json::array({v.x,v.y,v.z});}
json Encode(const Frame& f) {
    ValidateFrame(f);
    json entities=json::array();
    for(const auto& e:f.entities) entities.push_back({{"id",e.id},{"kind",Kind(e.kind)},{"name",e.name},
        {"position",Vector(e.position)},{"top",Vector(e.top)},
        {"look_direction",e.lookDirection?Vector(*e.lookDirection):json(nullptr)}});
    return {{"schema_version",1},{"sequence",f.sequence},{"timestamp_us",f.timestampUs},{"source",f.source},
        {"camera",{{"position",Vector(f.camera.position)},{"view_projection",f.camera.viewProjection.m}}},{"entities",std::move(entities)}};
}
Frame Decode(const json& j) {
    Require(Unsigned(j.at("schema_version"))==1,"unsupported frame schema");
    Frame f;f.sequence=Unsigned(j.at("sequence"));f.timestampUs=Unsigned(j.at("timestamp_us"));f.source=j.at("source").get<std::string>();
    const auto& camera=j.at("camera");f.camera.position=Vector(camera.at("position"));
    const auto& m=camera.at("view_projection");Require(m.is_array()&&m.size()==16,"matrix requires 16 components");
    for(std::size_t i=0;i<16;++i) {Require(m[i].is_number(),"matrix component must be numeric");f.camera.viewProjection.m[i]=m[i].get<float>();}
    const auto& entities=j.at("entities");Require(entities.is_array()&&entities.size()<=MaxFrameEntities,"entity count exceeds limit");
    f.entities.reserve(entities.size());
    for(const auto& item:entities) {
        Entity e;e.id=Unsigned(item.at("id"));e.kind=ParseKind(item.at("kind").get<std::string>());
        e.name=item.at("name").get<std::string>();e.position=Vector(item.at("position"));e.top=Vector(item.at("top"));
        if(item.contains("look_direction")&&!item.at("look_direction").is_null()) e.lookDirection=Vector(item.at("look_direction"));
        f.entities.push_back(std::move(e));
    }
    ValidateFrame(f);return f;
}
json Parse(std::string_view text,std::size_t limit) {
    Require(text.size()<=limit,"JSON payload exceeds size limit");
    return json::parse(text.begin(),text.end(),[](int depth,json::parse_event_t,json&) {
        Require(depth<=32,"JSON nesting exceeds limit");return true;
    });
}
}
void ValidateFrame(const Frame& f) {
    Require(FinitePosition(f.camera.position),"invalid camera position");
    Require(ValidMatrix(f.camera.viewProjection),"nonfinite, singular or unbounded camera matrix");
    Require(ValidText(f.source,256),"invalid source label");
    Require(f.entities.size()<=MaxFrameEntities,"entity count exceeds limit");
    std::unordered_set<std::uint64_t> ids;ids.reserve(f.entities.size());
    for(const auto& e:f.entities) {
        Require(e.id!=0&&ids.insert(e.id).second,"zero or duplicate entity ID");
        (void)Kind(e.kind);
        Require(ValidText(e.name,256),"invalid entity name");
        Require(FinitePosition(e.position)&&FinitePosition(e.top),"invalid entity coordinates");
        if(e.lookDirection) Require(FinitePosition(*e.lookDirection)&&std::abs(Length(*e.lookDirection)-1)<=0.01f,"look direction is not normalized");
    }
}
std::string SerializeFrame(const Frame& f) {
    auto text=Encode(f).dump();Require(text.size()<=MaxFrameBytes,"frame exceeds JSON size limit");return text;
}
Frame DeserializeFrame(std::string_view text) {return Decode(Parse(text,MaxFrameBytes));}
void AtomicWriteText(const std::filesystem::path& path,std::string_view text) {
    Require(text.size()<=MaxFrameBytes+1024,"atomic payload exceeds size limit");
    auto temporary=path;temporary+=".tmp";
    {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out.exceptions(std::ios::failbit|std::ios::badbit);out.write(text.data(),static_cast<std::streamsize>(text.size()));out.close();}
#ifdef _WIN32
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(500);
    for(;;) {
        if(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) break;
        const auto error=GetLastError();
        if((error!=ERROR_SHARING_VIOLATION&&error!=ERROR_ACCESS_DENIED)||std::chrono::steady_clock::now()>=deadline)
            throw std::system_error(static_cast<int>(error),std::system_category(),"atomic file replacement");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
#else
    std::filesystem::rename(temporary,path);
#endif
}

RecordedFrame DeserializeRecording(std::string_view text) {
    const auto j=Parse(text,MaxFrameBytes+1024);
    Require(Unsigned(j.at("recording_version"))==1,"unsupported recording schema");
    RecordedFrame r;r.session=j.at("session").get<std::string>();Require(!r.session.empty()&&ValidText(r.session,128),"invalid recording session");
    r.elapsedUs=Unsigned(j.at("elapsed_us"));r.frame=Decode(j.at("frame"));return r;
}
FrameRecorder::FrameRecorder(const std::filesystem::path& path):path_(path),start_(std::chrono::steady_clock::now()) {
    static std::atomic<unsigned> serial{};
    session_=std::to_string(start_.time_since_epoch().count())+"-"+std::to_string(serial.fetch_add(1));
    if(std::filesystem::exists(path_)) {
        bytes_=std::filesystem::file_size(path_);Require(bytes_<MaxRecordingBytes,"recording reached size limit");
        if(bytes_) {
            std::ifstream check(path_,std::ios::binary);std::string first;
            // Reading is bounded even for an unrelated or truncated destination.
            for(char c;check.get(c)&&c!='\n';) {first+=c;Require(first.size()<=MaxFrameBytes+1024,"recording line too large");}
            (void)DeserializeRecording(first);
            check.clear();check.seekg(-1,std::ios::end);char last{};check.get(last);Require(last=='\n',"recording has an incomplete last line");
        }
    }
    out_.exceptions(std::ios::failbit|std::ios::badbit);out_.open(path_,std::ios::binary|std::ios::app);
}
void FrameRecorder::Write(const Frame& f,std::chrono::steady_clock::time_point now) {
    Require(now>=start_,"recording time precedes session start");
    const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(now-start_).count();
    Require(static_cast<std::uint64_t>(elapsed)>=lastElapsed_,"recording time regressed");
    const auto text=json{{"recording_version",1},{"session",session_},{"elapsed_us",elapsed},{"frame",Encode(f)}}.dump();
    Require(text.size()<=MaxFrameBytes+1024,"recording line exceeds limit");
    Require(bytes_+text.size()+1<=MaxRecordingBytes,"recording reached size limit");
    out_<<text<<'\n';out_.flush();bytes_+=text.size()+1;lastElapsed_=static_cast<std::uint64_t>(elapsed);
}
}
