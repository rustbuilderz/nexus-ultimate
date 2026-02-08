#pragma once

#include <d3d11.h>
#include <string>
#include <wincodec.h>
#include <wrl/client.h>

namespace Render {

    struct LoadedTexture {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        int w = 0;
        int h = 0;

        explicit operator bool() const { return srv != nullptr; }
    };

    struct WicLoader {
        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;

        bool Init();
        void Shutdown();

        LoadedTexture LoadTextureBGRA(ID3D11Device* device, const std::wstring& filename);
    };

} // namespace Render
