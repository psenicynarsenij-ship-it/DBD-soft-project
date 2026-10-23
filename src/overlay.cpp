#include <filesystem>
#include "esp/overlay.hpp"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <stdexcept>
#include <sstream>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace esp {
static void Check(HRESULT hr,const char* operation) {
    if(FAILED(hr)) {std::ostringstream s;s<<operation<<" failed (HRESULT 0x"<<std::hex<<static_cast<unsigned long>(hr)<<")";throw std::runtime_error(s.str());}
}
std::filesystem::path ExecutableDirectory() {
    std::vector<wchar_t> buffer(32768);
    const auto n=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
    if(!n||n>=buffer.size()) throw std::runtime_error("Cannot determine executable directory");
    return std::filesystem::path(std::wstring(buffer.data(),n)).parent_path();
}
void Overlay::FitPrimaryMonitor() {
    const POINT origin{};
    MONITORINFO info{};info.cbSize=sizeof(info);
    if(!GetMonitorInfoW(MonitorFromPoint(origin,MONITOR_DEFAULTTOPRIMARY),&info)) throw std::runtime_error("GetMonitorInfo failed");
    const auto& r=info.rcMonitor;
    SetWindowPos(window_,HWND_TOPMOST,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE);
}
void Overlay::Initialize(HINSTANCE instance) {
    instance_=instance;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=WindowProc;wc.hInstance=instance;
    wc.lpszClassName=L"VisualESPDemoOverlay";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    if(!RegisterClassExW(&wc)) throw std::runtime_error("RegisterClassExW failed");
    window_=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOREDIRECTIONBITMAP|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE,
        wc.lpszClassName,L"Visual ESP - DEMO",WS_POPUP,0,0,1,1,nullptr,nullptr,instance,this);
    if(!window_) throw std::runtime_error("CreateWindowExW failed");
    FitPrimaryMonitor();
    RECT rect{};GetClientRect(window_,&rect);width_=rect.right;height_=rect.bottom;
    const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels,1,D3D11_SDK_VERSION,device_.GetAddressOf(),nullptr,context_.GetAddressOf());
    if(FAILED(hr)) Check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels,1,D3D11_SDK_VERSION,device_.GetAddressOf(),nullptr,context_.GetAddressOf()),"D3D11CreateDevice (WARP)");
    ComPtr<IDXGIDevice> dxgiDevice;Check(device_.As(&dxgiDevice),"Query IDXGIDevice");
    ComPtr<IDXGIAdapter> adapter;Check(dxgiDevice->GetAdapter(adapter.GetAddressOf()),"GetAdapter");
    ComPtr<IDXGIFactory2> factory;Check(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf())),"Get DXGI factory");
    DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=width_;desc.Height=height_;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;
    desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
    Check(factory->CreateSwapChainForComposition(device_.Get(),&desc,nullptr,swap_.GetAddressOf()),"CreateSwapChainForComposition");
    Check(DCompositionCreateDevice(dxgiDevice.Get(),IID_PPV_ARGS(composition_.GetAddressOf())),"DCompositionCreateDevice");
    Check(composition_->CreateTargetForHwnd(window_,TRUE,compositionTarget_.GetAddressOf()),"CreateTargetForHwnd");
    Check(composition_->CreateVisual(visual_.GetAddressOf()),"CreateVisual");
    Check(visual_->SetContent(swap_.Get()),"SetContent");
    Check(compositionTarget_->SetRoot(visual_.Get()),"SetRoot");Check(composition_->Commit(),"Composition Commit");
    CreateTarget();
    IMGUI_CHECKVERSION();ImGui::CreateContext();imguiContext_=true;
    ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    win32Backend_=ImGui_ImplWin32_Init(window_);if(!win32Backend_) throw std::runtime_error("ImGui Win32 init failed");
    dx11Backend_=ImGui_ImplDX11_Init(device_.Get(),context_.Get());if(!dx11Backend_) throw std::runtime_error("ImGui DX11 init failed");
    ShowWindow(window_,SW_SHOWNOACTIVATE);UpdateWindow(window_);
    pendingWidth_=pendingHeight_=0;
}
void Overlay::CreateTarget() {
    ComPtr<ID3D11Texture2D> back;
    Check(swap_->GetBuffer(0,IID_PPV_ARGS(back.GetAddressOf())),"GetBuffer");
    Check(device_->CreateRenderTargetView(back.Get(),nullptr,target_.GetAddressOf()),"CreateRenderTargetView");
}
Overlay::~Overlay() {
    if(dx11Backend_) ImGui_ImplDX11_Shutdown();
    if(win32Backend_) ImGui_ImplWin32_Shutdown();
    if(imguiContext_) ImGui::DestroyContext();
    if(context_) context_->ClearState();
    if(window_) DestroyWindow(window_);
    if(instance_) UnregisterClassW(L"VisualESPDemoOverlay",instance_);
}
void Overlay::SetInteractive(bool enabled) {
    if(enabled==interactive_) return;
    interactive_=enabled;
    LONG_PTR style=GetWindowLongPtrW(window_,GWL_EXSTYLE);
    if(enabled) style&=~static_cast<LONG_PTR>(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE);
    else style|=WS_EX_TRANSPARENT|WS_EX_NOACTIVATE;
    SetWindowLongPtrW(window_,GWL_EXSTYLE,style);
    SetWindowPos(window_,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED|SWP_NOACTIVATE);
    if(enabled) SetForegroundWindow(window_);
}
bool Overlay::Poll() {
    MSG msg{};
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
        if(msg.message==WM_QUIT) return false;
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    return true;
}
bool Overlay::BeginFrame() {
    if(pendingWidth_&&pendingHeight_) {
        context_->OMSetRenderTargets(0,nullptr,nullptr);target_.Reset();
        Check(swap_->ResizeBuffers(0,pendingWidth_,pendingHeight_,DXGI_FORMAT_UNKNOWN,0),"ResizeBuffers");
        width_=static_cast<int>(pendingWidth_);height_=static_cast<int>(pendingHeight_);
        pendingWidth_=pendingHeight_=0;CreateTarget();
    }
    if(IsIconic(window_)) {Sleep(20);return false;}
    ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();return true;
}
void Overlay::EndFrame(const std::filesystem::path& capture) {
    ImGui::Render();
    const float clear[4]={0,0,0,0};
    context_->OMSetRenderTargets(1,target_.GetAddressOf(),nullptr);context_->ClearRenderTargetView(target_.Get(),clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    if(!capture.empty()) CaptureBackbuffer(capture);
    const HRESULT result=swap_->Present(1,0);
    if(result==DXGI_STATUS_OCCLUDED) Sleep(20);else Check(result,"Present");
}
LRESULT CALLBACK Overlay::WindowProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    auto* self=reinterpret_cast<Overlay*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {
        self=static_cast<Overlay*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    if(self&&self->interactive_&&ImGui::GetCurrentContext()&&ImGui_ImplWin32_WndProcHandler(hwnd,msg,w,l)) return TRUE;
    switch(msg) {
    case WM_NCHITTEST: if(self&&!self->interactive_) return HTTRANSPARENT;break;
    case WM_MOUSEACTIVATE: if(self&&!self->interactive_) return MA_NOACTIVATE;break;
    case WM_SIZE:
        if(self&&w!=SIZE_MINIMIZED) {self->pendingWidth_=LOWORD(l);self->pendingHeight_=HIWORD(l);}return 0;
    case WM_DISPLAYCHANGE: if(self&&self->window_) self->FitPrimaryMonitor();return 0;
    case WM_DPICHANGED: if(self&&self->window_) self->FitPrimaryMonitor();return 0;
    case WM_CLOSE: PostQuitMessage(0);return 0;
    case WM_DESTROY: PostQuitMessage(0);return 0;
    case WM_SYSCOMMAND: if((w&0xfff0)==SC_KEYMENU) return 0;break;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
}
