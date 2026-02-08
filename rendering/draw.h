#pragma once

#include "rendering/dx11_context.h"
#include "rendering/wic_texture_loader.h"

#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Render {

    struct TextureItem {
        std::wstring path;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    struct Draw {
        HWND hwnd = nullptr;

        Dx11Context dx;
        WicLoader   wic;

        std::vector<TextureItem> textures;
        std::vector<int> visibleIndices;

        int page = 0; // 0=Home, 1=Choose
        float downForce = 0.0f;
        float upForce = 0.0f;
        float rightDrift = 0.0f;
        float leftDrift = 0.0f;
        int delayMs = 0;

        bool toggleBox = false;
        bool enableOnlyAvailable = false;

        int selectedIndex = -1;

        std::thread worker;

        bool Init(HWND h);
        void Shutdown();

        void Reload();
        void RebuildVisible();

        void TickUI();

        void OnResize(UINT w, UINT h);
        void OnF5();
    };

} // namespace Render
