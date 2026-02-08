#include "pch.h"
#include "wic_texture_loader.h"

#include <vector>

namespace Render {

    bool WicLoader::Init() {
        if (factory) return true;

        const HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(factory.GetAddressOf()));

        return SUCCEEDED(hr);
    }

    void WicLoader::Shutdown() {
        factory.Reset();
    }

    LoadedTexture WicLoader::LoadTextureBGRA(ID3D11Device* device, const std::wstring& filename) {
        LoadedTexture out{};
        if (!factory || !device) return out;

        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory->CreateDecoderFromFilename(
            filename.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            decoder.GetAddressOf());
        if (FAILED(hr)) return out;

        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, frame.GetAddressOf());
        if (FAILED(hr)) return out;

        UINT w = 0, h = 0;
        if (FAILED(frame->GetSize(&w, &h)) || w == 0 || h == 0) return out;

        Microsoft::WRL::ComPtr<IWICFormatConverter> conv;
        hr = factory->CreateFormatConverter(conv.GetAddressOf());
        if (FAILED(hr)) return out;

        hr = conv->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) return out;

        std::vector<uint8_t> pixels(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);

        const UINT stride = w * 4;
        const UINT bytes = static_cast<UINT>(pixels.size());

        hr = conv->CopyPixels(nullptr, stride, bytes, pixels.data());
        if (FAILED(hr)) return out;

        D3D11_TEXTURE2D_DESC td{};
        td.Width = w;
        td.Height = h;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sub{};
        sub.pSysMem = pixels.data();
        sub.SysMemPitch = stride;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        hr = device->CreateTexture2D(&td, &sub, tex.GetAddressOf());
        if (FAILED(hr)) return out;

        hr = device->CreateShaderResourceView(tex.Get(), nullptr, out.srv.GetAddressOf());
        if (FAILED(hr)) return out;

        out.w = static_cast<int>(w);
        out.h = static_cast<int>(h);
        return out;
    }

} // namespace Render
