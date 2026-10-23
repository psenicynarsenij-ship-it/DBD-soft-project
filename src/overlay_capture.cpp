#include "esp/overlay.hpp"
#include <fstream>
#include <stdexcept>
namespace esp {
void Overlay::CaptureBackbuffer(const std::filesystem::path& path) {
    ComPtr<ID3D11Texture2D> buffer,staging;
    if(FAILED(swap_->GetBuffer(0,IID_PPV_ARGS(buffer.GetAddressOf()))))throw std::runtime_error("Capture: GetBuffer failed");
    D3D11_TEXTURE2D_DESC desc{};buffer->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    if(FAILED(device_->CreateTexture2D(&desc,nullptr,&staging)))throw std::runtime_error("Capture: staging texture failed");
    context_->CopyResource(staging.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE map{};
    if(FAILED(context_->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)))throw std::runtime_error("Capture: Map failed");
    struct Unmap {ID3D11DeviceContext* c;ID3D11Texture2D* t;~Unmap(){c->Unmap(t,0);}} unmap{context_.Get(),staging.Get()};
    if(desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM && desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)throw std::runtime_error("Capture: unsupported pixel format");
    BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};file.bfType=0x4d42;file.bfOffBits=sizeof file+sizeof info;file.bfSize=file.bfOffBits+desc.Width*desc.Height*4;
    info.biSize=sizeof info;info.biWidth=static_cast<LONG>(desc.Width);info.biHeight=-static_cast<LONG>(desc.Height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=desc.Width*desc.Height*4;
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof file);out.write(reinterpret_cast<const char*>(&info),sizeof info);
    for(UINT y=0;y<desc.Height;++y)out.write(static_cast<const char*>(map.pData)+y*map.RowPitch,desc.Width*4);
    if(!out)throw std::runtime_error("Capture: cannot write BMP");
}
}
