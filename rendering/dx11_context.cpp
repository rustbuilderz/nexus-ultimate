#include "pch.h"
#include "dx11_context.h"

#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/imgui.h"

namespace {

    void SafeGetSwapDesc(IDXGISwapChain* swap, DXGI_SWAP_CHAIN_DESC& out) {
        ZeroMemory(&out, sizeof(out));
        if (swap) swap->GetDesc(&out);
    }

} // namespace

namespace Render {

    bool Dx11Context::Create(HWND hwnd) {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        const D3D_FEATURE_LEVEL requested[] = { D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL actual = D3D_FEATURE_LEVEL_11_0;

        const HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            requested,
            1,
            D3D11_SDK_VERSION,
            &sd,
            swap.GetAddressOf(),
            device.GetAddressOf(),
            &actual,
            ctx.GetAddressOf());

        if (FAILED(hr)) return false;

        DXGI_SWAP_CHAIN_DESC got{};
        SafeGetSwapDesc(swap.Get(), got);
        backbufferFormat = got.BufferDesc.Format;

        CreateMainRTV();
        CreateMSAATarget();
        return true;
    }

    void Dx11Context::Resize(UINT w, UINT h) {
        if (!device || !swap || w == 0 || h == 0) return;

        msaaRTV.Reset();
        msaaTex.Reset();
        rtv.Reset();

        swap->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);

        DXGI_SWAP_CHAIN_DESC got{};
        SafeGetSwapDesc(swap.Get(), got);
        backbufferFormat = got.BufferDesc.Format;

        CreateMainRTV();
        CreateMSAATarget();
    }

    void Dx11Context::CreateMainRTV() {
        if (!device || !swap) return;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> back;
        if (FAILED(swap->GetBuffer(0, IID_PPV_ARGS(&back)))) return;

        device->CreateRenderTargetView(back.Get(), nullptr, rtv.GetAddressOf());
    }

    void Dx11Context::CreateMSAATarget() {
        msaaRTV.Reset();
        msaaTex.Reset();

        if (!device || !swap) return;

        DXGI_SWAP_CHAIN_DESC sd{};
        SafeGetSwapDesc(swap.Get(), sd);

        msaaQuality = 0;
        const HRESULT qhr = device->CheckMultisampleQualityLevels(
            sd.BufferDesc.Format, msaaCount, &msaaQuality);

        if (FAILED(qhr) || msaaQuality == 0) {
            msaaCount = 1;
            return;
        }

        D3D11_TEXTURE2D_DESC td{};
        td.Width = sd.BufferDesc.Width;
        td.Height = sd.BufferDesc.Height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = sd.BufferDesc.Format;
        td.SampleDesc.Count = msaaCount;
        td.SampleDesc.Quality = msaaQuality - 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET;

        HRESULT hr = device->CreateTexture2D(&td, nullptr, msaaTex.GetAddressOf());
        if (FAILED(hr)) {
            msaaCount = 1;
            return;
        }

        hr = device->CreateRenderTargetView(msaaTex.Get(), nullptr, msaaRTV.GetAddressOf());
        if (FAILED(hr)) {
            msaaRTV.Reset();
            msaaTex.Reset();
            msaaCount = 1;
        }
    }

    void Dx11Context::BeginFrame(const float clear[4]) {
        if (!ctx) return;

        ID3D11RenderTargetView* rt =
            (msaaRTV && msaaCount > 1) ? msaaRTV.Get() : rtv.Get();

        if (!rt) return;

        ctx->OMSetRenderTargets(1, &rt, nullptr);
        ctx->ClearRenderTargetView(rt, clear);
    }

    void Dx11Context::RenderImGuiDrawData() {
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void Dx11Context::EndFrame() {
        if (!swap || !ctx) return;

        if (msaaRTV && msaaTex && msaaCount > 1) {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> back;
            if (SUCCEEDED(swap->GetBuffer(0, IID_PPV_ARGS(&back)))) {
                ctx->ResolveSubresource(back.Get(), 0, msaaTex.Get(), 0, backbufferFormat);
            }
        }

        swap->Present(1, 0);
    }

} // namespace Render
