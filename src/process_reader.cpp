#include "esp/process_reader.hpp"
#include <algorithm>
#include <limits>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif
namespace esp {
MemoryReadError::MemoryReadError(std::string m,std::uint64_t a,std::size_t n,std::size_t d,unsigned long e)
:std::runtime_error(std::move(m)+" (Win32="+std::to_string(e)+", address="+std::to_string(a)+", bytes="+std::to_string(d)+"/"+std::to_string(n)+")"),address(a),requested(n),transferred(d),code(e) {}
#ifdef _WIN32
namespace {
struct Handle { HANDLE h=INVALID_HANDLE_VALUE; ~Handle(){if(h && h!=INVALID_HANDLE_VALUE) CloseHandle(h);} };
std::wstring Wide(const std::string& s) {
    if(s.empty() || s.size()>32767) throw std::invalid_argument("Invalid process/module name");
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(!n) throw std::invalid_argument("Invalid UTF-8 process/module name");
    std::wstring r(n,L' '); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),r.data(),n); return r;
}
[[noreturn]] void Fail(const char* action,DWORD error=GetLastError()) {throw MemoryReadError(action,0,0,0,error);}
}
struct ProcessReader::Impl { Handle process; DWORD pid{}; };
ProcessReader::ProcessReader():impl_(std::make_unique<Impl>()){}
ProcessReader::~ProcessReader()=default;
void ProcessReader::Reset() noexcept { if(impl_->process.h!=INVALID_HANDLE_VALUE && impl_->process.h) CloseHandle(impl_->process.h); impl_->process.h=INVALID_HANDLE_VALUE; impl_->pid=0; }
void ProcessReader::Attach(const std::string& executable,std::uint32_t pid) {
    Reset(); const auto expected=Wide(executable);
    Handle snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0)};
    if(snapshot.h==INVALID_HANDLE_VALUE) Fail("CreateToolhelp32Snapshot(process)");
    PROCESSENTRY32W entry{}; entry.dwSize=sizeof entry; DWORD found=0;
    if(!Process32FirstW(snapshot.h,&entry)) Fail("Process32FirstW");
    do { if((!pid || pid==entry.th32ProcessID) && _wcsicmp(expected.c_str(),entry.szExeFile)==0) {
        if(found) throw std::runtime_error("Multiple matching processes; specify pid in memory layout");
        found=entry.th32ProcessID;
    }} while(Process32NextW(snapshot.h,&entry));
    if(!found) throw std::runtime_error("Process not found: "+executable);
    Handle opened{OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,found)};
    if(!opened.h) Fail("OpenProcess(read/query)");
    BOOL wow=FALSE;
    if(!IsWow64Process(opened.h,&wow)) Fail("IsWow64Process");
    if(wow || sizeof(void*)!=8) throw std::runtime_error("Only native Windows x64 processes are supported");
    // Revalidate the executable after opening: a process could exit and its PID be reused.
    wchar_t path[32768]; DWORD length=32768;
    if(!QueryFullProcessImageNameW(opened.h,0,path,&length)) Fail("QueryFullProcessImageNameW");
    std::wstring actual(path,length); auto slash=actual.find_last_of(L"\\/");
    if(_wcsicmp(actual.substr(slash==std::wstring::npos?0:slash+1).c_str(),expected.c_str())!=0) throw std::runtime_error("Process identity changed during attachment");
    impl_->process.h=opened.h; opened.h=INVALID_HANDLE_VALUE; impl_->pid=found;
}
bool ProcessReader::Alive() const noexcept {DWORD code=0; return impl_->process.h && impl_->process.h!=INVALID_HANDLE_VALUE && GetExitCodeProcess(impl_->process.h,&code) && code==STILL_ACTIVE;}
std::uint32_t ProcessReader::Pid() const noexcept{return impl_->pid;}
void ProcessReader::Read(std::uint64_t address,void* output,std::size_t bytes) const {
    constexpr std::size_t MaxRead=8*1024*1024;
    SYSTEM_INFO si{}; GetNativeSystemInfo(&si);
    const auto low=reinterpret_cast<std::uintptr_t>(si.lpMinimumApplicationAddress),high=reinterpret_cast<std::uintptr_t>(si.lpMaximumApplicationAddress);
    if(!output || !bytes || bytes>MaxRead || address<low || address>high || bytes-1>high-address) throw MemoryReadError("Read bounds rejected",address,bytes,0,ERROR_INVALID_PARAMETER);
    std::size_t done=0;
    while(done<bytes) {
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQueryEx(impl_->process.h,reinterpret_cast<LPCVOID>(address+done),&region,sizeof region)) throw MemoryReadError("VirtualQueryEx",address,bytes,done,GetLastError());
        const DWORD p=region.Protect&0xff;
        if(region.State!=MEM_COMMIT || (region.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || !(p==PAGE_READONLY || p==PAGE_READWRITE || p==PAGE_WRITECOPY || p==PAGE_EXECUTE_READ || p==PAGE_EXECUTE_READWRITE || p==PAGE_EXECUTE_WRITECOPY)) throw MemoryReadError("Unreadable memory region",address,bytes,done,ERROR_NOACCESS);
        auto end=reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize;
        if(end<=address+done) throw MemoryReadError("Invalid memory region",address,bytes,done,ERROR_INVALID_ADDRESS);
        auto amount=std::min<std::size_t>(bytes-done,end-address-done); SIZE_T got=0;
        BOOL ok=ReadProcessMemory(impl_->process.h,reinterpret_cast<LPCVOID>(address+done),static_cast<char*>(output)+done,amount,&got);
        DWORD error=ok?ERROR_PARTIAL_COPY:GetLastError(); done+=got;
        if(!ok || got!=amount) throw MemoryReadError("ReadProcessMemory",address,bytes,done,error);
    }
}
ModuleInfo ProcessReader::Module(const std::string& name) const {
    auto expected=Wide(name); Handle snapshot;
    for(int retry=0;retry<4;++retry) {snapshot.h=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,impl_->pid); if(snapshot.h!=INVALID_HANDLE_VALUE || GetLastError()!=ERROR_BAD_LENGTH) break;}
    if(snapshot.h==INVALID_HANDLE_VALUE) Fail("CreateToolhelp32Snapshot(module)");
    MODULEENTRY32W entry{};entry.dwSize=sizeof entry;
    if(!Module32FirstW(snapshot.h,&entry)) Fail("Module32FirstW");
    do {if(_wcsicmp(expected.c_str(),entry.szModule)==0) {
        ModuleInfo m{reinterpret_cast<std::uintptr_t>(entry.modBaseAddr),entry.modBaseSize};
        auto dos=Value<IMAGE_DOS_HEADER>(m.base);
        if(dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<0 || static_cast<std::uint64_t>(dos.e_lfanew)+sizeof(IMAGE_NT_HEADERS64)>m.size) throw std::runtime_error("Invalid module PE header");
        auto nt=Value<IMAGE_NT_HEADERS64>(m.base+dos.e_lfanew);
        if(nt.Signature!=IMAGE_NT_SIGNATURE || nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64) throw std::runtime_error("Expected AMD64 PE32+ module");
        m.timestamp=nt.FileHeader.TimeDateStamp;m.machine=nt.FileHeader.Machine;return m;
    }} while(Module32NextW(snapshot.h,&entry));
    throw std::runtime_error("Module not found: "+name);
}
std::uint64_t ProcessReader::Export(const ModuleInfo& m,const std::string& name) const {
    auto dos=Value<IMAGE_DOS_HEADER>(m.base); auto nt=Value<IMAGE_NT_HEADERS64>(m.base+dos.e_lfanew);
    auto dir=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    auto checked=[&](std::uint64_t rva,std::size_t size){if(!rva || rva>=m.size || size>m.size-rva) throw std::runtime_error("Export RVA outside module");return m.base+rva;};
    auto exp=Value<IMAGE_EXPORT_DIRECTORY>(checked(dir.VirtualAddress,sizeof(IMAGE_EXPORT_DIRECTORY)));
    if(exp.NumberOfNames>65536 || exp.NumberOfFunctions>65536) throw std::runtime_error("Export table too large");
    for(std::uint32_t i=0;i<exp.NumberOfNames;++i) {
        auto rva=Value<DWORD>(checked(static_cast<std::uint64_t>(exp.AddressOfNames)+i*4,4)); std::string text;
        for(std::size_t c=0;c<256;++c) {char ch=Value<char>(checked(static_cast<std::uint64_t>(rva)+c,1));if(!ch)break;text+=ch;}
        if(text!=name)continue;
        auto ordinal=Value<WORD>(checked(static_cast<std::uint64_t>(exp.AddressOfNameOrdinals)+i*2,2));
        if(ordinal>=exp.NumberOfFunctions) throw std::runtime_error("Invalid export ordinal");
        auto fn=Value<DWORD>(checked(static_cast<std::uint64_t>(exp.AddressOfFunctions)+ordinal*4,4));
        if(fn>=dir.VirtualAddress && static_cast<std::uint64_t>(fn)<static_cast<std::uint64_t>(dir.VirtualAddress)+dir.Size) throw std::runtime_error("Forwarded data exports unsupported");
        return checked(fn,8);
    }
    throw std::runtime_error("Data export not found: "+name);
}
#else
struct ProcessReader::Impl {};
ProcessReader::ProcessReader():impl_(std::make_unique<Impl>()){} ProcessReader::~ProcessReader()=default;
void ProcessReader::Reset() noexcept{} bool ProcessReader::Alive() const noexcept{return false;} std::uint32_t ProcessReader::Pid() const noexcept{return 0;}
void ProcessReader::Attach(const std::string&,std::uint32_t){throw std::runtime_error("Live memory requires Windows x64");}
void ProcessReader::Read(std::uint64_t a,void*,std::size_t n) const {throw MemoryReadError("Live memory requires Windows x64",a,n,0,50);}
ModuleInfo ProcessReader::Module(const std::string&) const {throw std::runtime_error("Live memory requires Windows x64");}
std::uint64_t ProcessReader::Export(const ModuleInfo&,const std::string&) const {throw std::runtime_error("Live memory requires Windows x64");}
#endif
}
