#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace esp {
struct MemoryReadError : std::runtime_error {
    std::uint64_t address; std::size_t requested,transferred; unsigned long code;
    MemoryReadError(std::string message,std::uint64_t a,std::size_t n,std::size_t done,unsigned long error);
};
class IMemoryReader {
public:
    virtual ~IMemoryReader()=default;
    virtual void Read(std::uint64_t address,void* destination,std::size_t bytes) const=0;
    template<class T> T Value(std::uint64_t address) const { T result{}; Read(address,&result,sizeof result); return result; }
};
struct ModuleInfo { std::uint64_t base{},size{}; std::uint32_t timestamp{}; std::uint16_t machine{}; };
class ProcessReader final : public IMemoryReader {
    struct Impl; std::unique_ptr<Impl> impl_;
public:
    ProcessReader(); ~ProcessReader();
    ProcessReader(const ProcessReader&)=delete; ProcessReader& operator=(const ProcessReader&)=delete;
    void Attach(const std::string& executable,std::uint32_t pid=0);
    void Reset() noexcept;
    bool Alive() const noexcept;
    std::uint32_t Pid() const noexcept;
    ModuleInfo Module(const std::string& name) const;
    std::uint64_t Export(const ModuleInfo&,const std::string& name) const;
    void Read(std::uint64_t,void*,std::size_t) const override;
};
}
