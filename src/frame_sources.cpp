#include "esp/frame_sources.hpp"
#include "esp/game_state_decoder.hpp"
#include <array>
#include <cmath>
#include <stdexcept>

namespace esp {
const char* SourceKindName(SourceKind kind) {
    switch(kind) {case SourceKind::Demo:return "DEMO";case SourceKind::JsonFile:return "LOCAL JSON (unverified producer)";case SourceKind::Replay:return "REPLAY";case SourceKind::LiveMemory:return "LIVE MEMORY (DBD unverified)";}
    return "UNKNOWN";
}
void ValidateRuntimeOptions(const RuntimeOptions& o) {
    if(o.kind!=SourceKind::Demo&&o.kind!=SourceKind::JsonFile&&o.kind!=SourceKind::Replay&&o.kind!=SourceKind::LiveMemory) throw std::runtime_error("unknown source kind");
    if(o.pollMs<5||o.pollMs>1000||o.staleMs<100||o.staleMs>30000) throw std::runtime_error("poll must be 5..1000 ms; stale timeout 100..30000 ms");
    for(const auto* p:{&o.jsonPath,&o.replayPath,&o.recordingPath,&o.memoryLayoutPath})
        if(p->empty()||p->size()>1023||p->find('\0')!=std::string::npos) throw std::runtime_error("data paths must contain 1..1023 UTF-8 bytes without NUL");
}
std::filesystem::path ResolveDataPath(const std::filesystem::path& base,const std::string& value) {
    const std::filesystem::path path(std::u8string(value.begin(),value.end()));return (path.is_absolute()?path:base/path).lexically_normal();
}
SourceRead DemoFrameSource::Poll(const ReadContext& c) {
    auto frame=demo_.Sample(c.aspect,c.camera);frame.sequence=++sequence_;
    frame.timestampUs=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(c.now.time_since_epoch()).count());
    return {ReadState::Ready,std::move(frame),{}};
}
SourceRead JsonFileSource::Poll(const ReadContext&) {
    try {
        if(!std::filesystem::is_regular_file(path_)) throw std::runtime_error("JSON source is missing or not a regular file");
        const auto modified=std::filesystem::last_write_time(path_);const auto size=std::filesystem::file_size(path_);
        if(modified_&&*modified_==modified&&size_==size)
            return {invalid_.empty()?ReadState::Unchanged:ReadState::Invalid,{},invalid_};
        modified_=modified;size_=size;invalid_.clear();
        try {
            if(size>MaxFrameBytes) throw std::runtime_error("JSON frame file exceeds 8 MiB");
            std::ifstream in(path_,std::ios::binary);if(!in) throw std::runtime_error("cannot open JSON frame file");
            std::string text(static_cast<std::size_t>(size),'\0');
            if(size&&!in.read(text.data(),static_cast<std::streamsize>(size))) throw std::runtime_error("incomplete JSON read");
            if(in.peek()!=std::char_traits<char>::eof()||std::filesystem::last_write_time(path_)!=modified)
                throw std::runtime_error("JSON changed while reading; use atomic replacement");
            in.close(); // Release the Windows file handle before parsing large snapshots.
            auto frame=DeserializeFrame(text);
            if(sequence_&&*sequence_==frame.sequence) return {ReadState::Unchanged,{},"waiting for a new sequence"};
            sequence_=frame.sequence;return {ReadState::Ready,std::move(frame),{}};
        } catch(const std::exception& e) {invalid_=e.what();sequence_.reset();return {ReadState::Invalid,{},invalid_};}
    } catch(const std::exception& e) {
        modified_.reset();sequence_.reset();invalid_.clear();return {ReadState::Disconnected,{},e.what()};
    }
}
namespace {
bool ReadLine(std::ifstream& in,std::string& text) {
    text.clear();auto* buffer=in.rdbuf();
    for(;;) {
        const auto value=buffer->sbumpc();
        if(value==std::char_traits<char>::eof()) {
            if(!text.empty()) throw std::runtime_error("recording has an incomplete final line");
            return false;
        }
        if(value=='\n') return true;
        if(text.size()>=MaxFrameBytes+1024) throw std::runtime_error("recording line exceeds limit");
        text.push_back(static_cast<char>(value));
    }
}
}
void ReplaySource::Reset() {
    in_.close();in_.clear();pending_.reset();modified_.reset();session_.clear();ended_=false;size_=0;previousElapsed_=firstElapsed_=0;
}
SourceRead ReplaySource::Poll(const ReadContext& c) {
    try {
        if(!std::filesystem::is_regular_file(path_)) {Reset();return {ReadState::Disconnected,{},"replay file is missing"};}
        const auto size=std::filesystem::file_size(path_);
        if(size>MaxRecordingBytes) throw std::runtime_error("replay exceeds 256 MiB");
        const auto modified=std::filesystem::last_write_time(path_);
        if(modified_&&(*modified_!=modified||size_!=size)) Reset();
        if(!in_.is_open()) {in_.open(path_,std::ios::binary);if(!in_) throw std::runtime_error("cannot open replay");modified_=modified;size_=size;}
        if(ended_) return {ReadState::Ended,{},"replay ended"};
        if(!pending_) {
            std::string text;
            if(!ReadLine(in_,text)) {
                if(loop_&&!session_.empty()) {Reset();return {ReadState::Scheduled,{},"looping replay"};}
                ended_=true;return {ReadState::Ended,{},"replay ended"};
            }
            auto entry=DeserializeRecording(text);
            if(session_!=entry.session) {session_=entry.session;start_=c.now;firstElapsed_=entry.elapsedUs;previousElapsed_=entry.elapsedUs;}
            if(entry.elapsedUs<previousElapsed_) throw std::runtime_error("replay time regressed within a session");
            previousElapsed_=entry.elapsedUs;pending_=std::move(entry);
        }
        const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(c.now-start_).count();
        if(elapsed<0||static_cast<std::uint64_t>(elapsed)<pending_->elapsedUs-firstElapsed_)
            return {ReadState::Scheduled,{},"replay waiting for timestamp"};
        Frame frame=std::move(pending_->frame);pending_.reset();return {ReadState::Ready,std::move(frame),{}};
    } catch(const std::exception& e) {Reset();return {ReadState::Invalid,{},e.what()};}
}
std::unique_ptr<IFrameSource> MakeFrameSource(const RuntimeOptions& o,const std::filesystem::path& base) {
    ValidateRuntimeOptions(o);
    if(o.record) {
        const auto output=ResolveDataPath(base,o.recordingPath);
        const auto input=ResolveDataPath(base,o.kind==SourceKind::LiveMemory?o.memoryLayoutPath:o.kind==SourceKind::Replay?o.replayPath:o.jsonPath);
        std::error_code ec;
        if(o.kind!=SourceKind::Demo&&(output==input||std::filesystem::equivalent(output,input,ec)))
            throw std::runtime_error("recording output must differ from source input");
    }
    switch(o.kind) {
    case SourceKind::LiveMemory:return std::make_unique<LiveFrameSource>(ResolveDataPath(base,o.memoryLayoutPath));
    case SourceKind::Demo:return std::make_unique<DemoFrameSource>();
    case SourceKind::JsonFile:return std::make_unique<JsonFileSource>(ResolveDataPath(base,o.jsonPath));
    case SourceKind::Replay:return std::make_unique<ReplaySource>(ResolveDataPath(base,o.replayPath),o.replayLoop);
    }
    throw std::runtime_error("unknown source kind");
}
}
