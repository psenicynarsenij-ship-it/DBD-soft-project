#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <wrl/client.h>
#include <filesystem>
#include <string>

namespace esp {
class Overlay {
    template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
    HWND window_{};
    HINSTANCE instance_{};
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain1> swap_;
    ComPtr<ID3D11RenderTargetView> target_;
    ComPtr<IDCompositionDevice> composition_;
    ComPtr<IDCompositionTarget> compositionTarget_;
    ComPtr<IDCompositionVisual> visual_;
    bool imguiContext_{}, win32Backend_{}, dx11Backend_{};
    bool interactive_{};
    UINT pendingWidth_{},pendingHeight_{};
    int width_{},height_{};
    static LRESULT CALLBACK WindowProc(HWND,UINT,WPARAM,LPARAM);
    void CreateTarget();
    void CaptureBackbuffer(const std::filesystem::path&);
    void FitPrimaryMonitor();
public:
    ~Overlay();
    void Initialize(HINSTANCE);
    bool Poll();
    void SetInteractive(bool);
    bool BeginFrame();
    void EndFrame(const std::filesystem::path& capture={});
    float Width() const {return static_cast<float>(width_);}
    float Height() const {return static_cast<float>(height_);}
};
std::filesystem::path ExecutableDirectory();
}
