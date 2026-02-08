#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace Render {

    struct Dx11Context {
        Microsoft::WRL::ComPtr<ID3D11Device>           device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext>    ctx;
        Microsoft::WRL::ComPtr<IDXGISwapChain>         swap;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;

        Microsoft::WRL::ComPtr<ID3D11Texture2D>        msaaTex;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> msaaRTV;

        UINT msaaCount = 4;
        UINT msaaQuality = 0;
        DXGI_FORMAT backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        bool Create(HWND hwnd);
        void Resize(UINT w, UINT h);

        void BeginFrame(const float clear[4]);
        void RenderImGuiDrawData();
        void EndFrame();

    private:
        void CreateMainRTV();
        void CreateMSAATarget();
    };

} // namespace Render
