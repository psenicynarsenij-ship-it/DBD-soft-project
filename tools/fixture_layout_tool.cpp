#include "../tests/support/fixture_contract.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::runtime_error("Usage: fixture_layout_tool EspMemoryFixture.exe memory.fixture.json");
        std::ifstream in(argv[1],std::ios::binary);if(!in)throw std::runtime_error("Cannot open fixture PE");
        std::vector<char> bytes((std::istreambuf_iterator<char>(in)),{});
        auto value=[&]<class T>(std::size_t offset){T r{};if(offset>bytes.size() || sizeof(T)>bytes.size()-offset)throw std::runtime_error("Truncated PE");std::memcpy(&r,bytes.data()+offset,sizeof r);return r;};
        if(value.operator()<std::uint16_t>(0)!=0x5a4d)throw std::runtime_error("Expected MZ");
        auto nt=value.operator()<std::uint32_t>(0x3c);
        if(value.operator()<std::uint32_t>(nt)!=0x4550 || value.operator()<std::uint16_t>(nt+4)!=0x8664 || value.operator()<std::uint16_t>(nt+24)!=0x20b)throw std::runtime_error("Expected AMD64 PE32+");
        auto j=fixture::Layout(value.operator()<std::uint32_t>(nt+24+56),value.operator()<std::uint32_t>(nt+8));
        std::ofstream out(argv[2]);out<<j.dump(2)<<'\n';if(!out)throw std::runtime_error("Cannot write layout");return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
