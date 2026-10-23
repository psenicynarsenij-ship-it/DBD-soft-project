#include "esp/game_state_decoder.hpp"
#include <cstring>
#include <algorithm>
#include <limits>
namespace esp {
namespace {
std::uint64_t Number(const nlohmann::json& j) {
    if(j.is_number_unsigned())return j.get<std::uint64_t>();
    if(j.is_number_integer()) {auto n=j.get<std::int64_t>();if(n>=0)return static_cast<std::uint64_t>(n);}
    if(j.is_string()) {const auto s=j.get<std::string>();std::size_t used{};if(s.empty() || s[0]=='-' || s[0]=='+')throw std::runtime_error("Invalid unsigned address");auto n=std::stoull(s,&used,0);if(used==s.size())return n;}
    throw std::runtime_error("Expected unsigned integer or hexadecimal address");
}
std::uint32_t U32(const nlohmann::json& j) {auto n=Number(j);if(n>UINT32_MAX)throw std::runtime_error("Layout integer overflow");return static_cast<std::uint32_t>(n);}
void Field(std::uint32_t offset,std::size_t bytes,std::uint32_t size) {if(offset>size || bytes>size-offset)throw std::runtime_error("Layout field outside structure");}
std::uint64_t Add(std::uint64_t a,std::uint64_t b) {if(b>UINT64_MAX-a)throw std::runtime_error("Memory address overflow");return a+b;}
template<class T> T Extract(const std::vector<char>& b,std::size_t offset) {T t{};if(offset>b.size() || sizeof(T)>b.size()-offset)throw std::runtime_error("Decoder buffer bounds");std::memcpy(&t,b.data()+offset,sizeof t);return t;}
}
MemoryLayout MemoryLayout::Parse(const nlohmann::json& j) {
    if(j.at("schema")!=1 || j.at("coordinate_contract")!="row-major-column-vector-lh-y-up-dx01")throw std::runtime_error("Unsupported memory layout/coordinate contract");
    MemoryLayout l; l.executable=j.at("executable");l.module=j.at("module");l.build=j.at("verified_build");l.rootExport=j.value("root_export",std::string{});
    if(l.executable.empty() || l.module.empty() || l.build.empty() || l.executable.size()>255 || l.module.size()>255 || l.rootExport.size()>255 || l.executable.find('\0')!=std::string::npos || l.module.find('\0')!=std::string::npos)throw std::runtime_error("Missing/invalid verified producer identity");
    l.pid=U32(j.value("pid",nlohmann::json(0)));l.imageSize=U32(j.at("image_size"));l.peTimestamp=U32(j.at("pe_timestamp"));
    if(!l.imageSize || j.at("machine")!="AMD64")throw std::runtime_error("Expected verified AMD64 PE fingerprint");
    l.rootRva=Number(j.value("root_rva",nlohmann::json(0)));
    if(l.rootExport.empty()==(l.rootRva==0))throw std::runtime_error("Specify exactly one root_export or root_rva");
    if(l.rootRva>=l.imageSize)throw std::runtime_error("Root RVA outside verified module");
    for(const auto& p:j.at("pointer_chain"))l.pointerChain.push_back(Number(p));
    if(l.pointerChain.size()>8)throw std::runtime_error("Pointer chain exceeds eight hops");
    auto& h=j.at("header");auto& e=j.at("entity");
    l.headerSize=U32(h.at("size"));l.stride=U32(e.at("stride"));l.maxEntities=U32(j.at("max_entities"));
    if(l.headerSize<16 || l.headerSize>65536 || !l.stride || l.stride>4096 || !l.maxEntities || l.maxEntities>10000 || static_cast<std::uint64_t>(l.stride)*l.maxEntities>8*1024*1024)throw std::runtime_error("Layout allocation limits exceeded");
    l.magic=Number(h.at("magic_value"));l.version=U32(h.at("version_value"));
#define HF(member,key,type) l.member=U32(h.at(key));Field(l.member,sizeof(type),l.headerSize)
    HF(magicOffset,"magic",std::uint64_t); HF(versionOffset,"version",std::uint32_t);HF(revisionOffset,"revision",std::uint64_t);HF(sequenceOffset,"sequence",std::uint64_t);HF(timestampOffset,"timestamp_us",std::uint64_t);HF(countOffset,"count",std::uint32_t);HF(cameraOffset,"camera_position",Vec3);HF(matrixOffset,"view_projection",Matrix4);
#undef HF
    l.entitiesOffset=U32(j.at("entities_offset"));l.entitiesPointer=j.value("entities_pointer",false);
    if(l.entitiesPointer)Field(l.entitiesOffset,8,l.headerSize);else if(l.entitiesOffset<l.headerSize || l.entitiesOffset>1024*1024)throw std::runtime_error("Invalid inline entity array offset");
#define EF(member,key,type) l.member=U32(e.at(key));Field(l.member,sizeof(type),l.stride)
    EF(idOffset,"id",std::uint64_t);EF(kindOffset,"kind",std::uint32_t);EF(positionOffset,"position",Vec3);EF(topOffset,"top",Vec3);EF(directionOffset,"direction",Vec3);EF(flagsOffset,"flags",std::uint32_t);
#undef EF
    l.directionMask=U32(e.at("direction_mask"));l.nameOffset=U32(e.at("name"));l.nameBytes=U32(e.at("name_bytes"));
    if(!l.nameBytes || l.nameBytes>256 || !l.directionMask)throw std::runtime_error("Invalid name/direction format");
    Field(l.nameOffset,l.nameBytes,l.stride);
    for(const auto& k:e.at("kinds"))l.kinds.push_back(U32(k));
    if(l.kinds.size()!=5)throw std::runtime_error("Exactly five kind mappings required");
    auto sorted=l.kinds;std::sort(sorted.begin(),sorted.end());if(std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end())throw std::runtime_error("Duplicate entity kind mapping");
    return l;
}
std::optional<Frame> GameStateDecoder::Read(const IMemoryReader& r,std::uint64_t root) const {
    const auto& l=layout_;
    const auto revisionAddress=Add(root,l.revisionOffset);
    if(revisionAddress%8)throw std::runtime_error("Revision must be aligned to eight bytes");
    const auto before=r.Value<std::uint64_t>(revisionAddress);if(before&1)return {};
    std::vector<char> header(l.headerSize);r.Read(root,header.data(),header.size());
    if(Extract<std::uint64_t>(header,l.magicOffset)!=l.magic || Extract<std::uint32_t>(header,l.versionOffset)!=l.version)throw std::runtime_error("Producer magic/version mismatch");
    auto revision=Extract<std::uint64_t>(header,l.revisionOffset);if(revision!=before || (revision&1))return {};
    auto count=Extract<std::uint32_t>(header,l.countOffset);if(count>l.maxEntities)throw std::runtime_error("Entity count exceeds verified capacity");
    auto address=l.entitiesPointer?Extract<std::uint64_t>(header,l.entitiesOffset):Add(root,l.entitiesOffset);
    std::vector<char> entities(static_cast<std::size_t>(count)*l.stride);if(count)r.Read(address,entities.data(),entities.size());
    if(r.Value<std::uint64_t>(Add(root,l.revisionOffset))!=revision)return {};
    Frame f;f.source="LIVE MEMORY (unverified game identity)";f.sequence=Extract<std::uint64_t>(header,l.sequenceOffset);f.timestampUs=Extract<std::uint64_t>(header,l.timestampOffset);f.camera.position=Extract<Vec3>(header,l.cameraOffset);f.camera.viewProjection=Extract<Matrix4>(header,l.matrixOffset);f.entities.reserve(count);
    constexpr EntityKind kinds[]={EntityKind::Survivor,EntityKind::Killer,EntityKind::Generator,EntityKind::Hook,EntityKind::ExitGate};
    for(std::uint32_t i=0;i<count;++i) {std::size_t o=static_cast<std::size_t>(i)*l.stride;Entity e;e.id=Extract<std::uint64_t>(entities,o+l.idOffset);auto k=Extract<std::uint32_t>(entities,o+l.kindOffset);auto it=std::find(l.kinds.begin(),l.kinds.end(),k);if(it==l.kinds.end())throw std::runtime_error("Unknown entity kind in memory");e.kind=kinds[it-l.kinds.begin()];e.position=Extract<Vec3>(entities,o+l.positionOffset);e.top=Extract<Vec3>(entities,o+l.topOffset);
        if(Extract<std::uint32_t>(entities,o+l.flagsOffset)&l.directionMask)e.lookDirection=Extract<Vec3>(entities,o+l.directionOffset);
        const char* name=entities.data()+o+l.nameOffset;auto end=static_cast<const char*>(std::memchr(name,0,l.nameBytes));if(!end)throw std::runtime_error("Unterminated entity name");e.name.assign(name,end);f.entities.push_back(std::move(e));}
    ValidateFrame(f);return f;
}
SourceRead LiveFrameSource::Poll(const ReadContext&) {
    ReadState failure=ReadState::Invalid;
    try {
        if(!std::filesystem::is_regular_file(path_))throw std::filesystem::filesystem_error("Memory layout must be a regular file",path_,std::make_error_code(std::errc::no_such_file_or_directory));
        auto stamp=std::filesystem::last_write_time(path_);
        if(!layout_ || !modified_ || stamp!=*modified_) {
            process_.Reset();sequence_.reset();layout_.reset();
            auto size=std::filesystem::file_size(path_);if(size>65536)throw std::runtime_error("Memory layout exceeds 64 KiB");
            std::ifstream input(path_,std::ios::binary);if(!input)throw std::runtime_error("Cannot open memory layout");
            auto json=nlohmann::json::parse(input,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&) {if(depth>32)throw std::runtime_error("Memory layout nesting exceeds 32 levels");return true;});layout_=MemoryLayout::Parse(json);modified_=stamp;
        }
        auto& l=*layout_;
        if(!process_.Alive()) {
            process_.Reset();sequence_.reset();failure=ReadState::Disconnected;process_.Attach(l.executable,l.pid);
            failure=ReadState::Invalid;
            auto m=process_.Module(l.module);if(m.size!=l.imageSize || m.timestamp!=l.peTimestamp || m.machine!=0x8664)throw std::runtime_error("Module fingerprint differs from verified memory layout");
            root_=l.rootExport.empty()?Add(m.base,l.rootRva):process_.Export(m,l.rootExport);
            for(auto offset:l.pointerChain)root_=process_.Value<std::uint64_t>(Add(root_,offset));
            if(!root_)throw std::runtime_error("Null root pointer");
        }
        auto frame=GameStateDecoder(l).Read(process_,root_);
        std::string status="PID "+std::to_string(process_.Pid())+" | "+l.build+" | read-only; DBD not verified";
        if(!frame)return {ReadState::Unchanged,{},"Producer snapshot in progress"};
        if(sequence_ && *sequence_==frame->sequence)return {ReadState::Unchanged,{},status};
        sequence_=frame->sequence;return {ReadState::Ready,std::move(frame),status};
    } catch(const MemoryReadError& e) {process_.Reset();sequence_.reset();return {ReadState::Disconnected,{},e.what()};}
      catch(const std::filesystem::filesystem_error& e) {process_.Reset();layout_.reset();sequence_.reset();return {ReadState::Disconnected,{},e.what()};}
      catch(const std::exception& e) {process_.Reset();sequence_.reset();return {failure,{},e.what()};}
}
}
