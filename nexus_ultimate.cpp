#define WIN32_LEAN_AND_MEAN
#include "pch.h"
#include <windows.h>
#include "rendering/draw.h"
#include "rendering/operator_config_window.h"
#include "console_alloc.h"
#include "worker.h"
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#include <pdh.h>
#pragma comment(lib, "pdh.lib")

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <glad/glad.h>

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"

#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include "imgui/backends/imgui_impl_opengl3.h"

static void glfw_error_callback(int error, const char* description)
{
    char buf[512];
    std::snprintf(buf, sizeof(buf), "GLFW Error %d: %s\n", error, description);
    OutputDebugStringA(buf);
}

static void ApplyRoundedWindowRegion(GLFWwindow* window, int radius)
{
    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd) return;

    int w = 0, h = 0;
    glfwGetWindowSize(window, &w, &h);

    HRGN rgn = CreateRoundRectRgn(
        0, 0, w + 1, h + 1,
        radius * 2, radius * 2
    );

    SetWindowRgn(hwnd, rgn, TRUE);
}
namespace {

    inline double GetProcessCPU()
    {
        static ULONGLONG lastTime = 0;
        static ULONGLONG lastProcTime = 0;
        static HANDLE self = GetCurrentProcess();
        static bool init = false;

        FILETIME ftCreation, ftExit, ftKernel, ftUser;
        GetProcessTimes(self, &ftCreation, &ftExit, &ftKernel, &ftUser);

        ULONGLONG procTime =
            ((ULONGLONG)ftKernel.dwLowDateTime | ((ULONGLONG)ftKernel.dwHighDateTime << 32)) +
            ((ULONGLONG)ftUser.dwLowDateTime | ((ULONGLONG)ftUser.dwHighDateTime << 32));

        ULONGLONG nowTime = GetTickCount64();

        if (!init) {
            init = true;
            lastTime = nowTime;
            lastProcTime = procTime;
            return 0.0;
        }

        ULONGLONG timeDiff = nowTime - lastTime;
        ULONGLONG procDiff = procTime - lastProcTime;

        lastTime = nowTime;
        lastProcTime = procTime;

        if (timeDiff == 0)
            return 0.0;

        SYSTEM_INFO si;
        GetSystemInfo(&si);

        return (double)procDiff / (double)timeDiff / si.dwNumberOfProcessors * 100.0;
    }
    inline double GetSystemCPU()
    {
        static PDH_HQUERY query = nullptr;
        static PDH_HCOUNTER counter;
        static bool init = false;

        if (!init)
        {
            PdhOpenQuery(nullptr, 0, &query);
            PdhAddEnglishCounterA(query, "\\Processor(_Total)\\% Processor Time", 0, &counter);
            PdhCollectQueryData(query);
            init = true;
            return 0.0;
        }

        PdhCollectQueryData(query);

        PDH_FMT_COUNTERVALUE value;
        PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value);

        return value.doubleValue;
    }
    inline void GetRAM(size_t& workingSetMB, size_t& privateMB)
    {
        PROCESS_MEMORY_COUNTERS_EX pmc{};
        GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));

        workingSetMB = pmc.WorkingSetSize / 1024 / 1024;
        privateMB = pmc.PrivateUsage / 1024 / 1024;
    }
    inline void LogSystemTelemetry(const char* tag)
    {
        double procCPU = GetProcessCPU();
        double sysCPU = GetSystemCPU();

        size_t wsMB = 0, privMB = 0;
        GetRAM(wsMB, privMB);

        LOGI("[SYS:%s] ProcCPU=%.2f%% SysCPU=%.2f%% RAM_WS=%zuMB RAM_PRIV=%zuMB",
            tag,
            procCPU,
            sysCPU,
            wsMB,
            privMB);
    }

}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    Console::AllocateConsoleOnce();
    Console::ToggleConsole();
    SetUnhandledExceptionFilter(Console::UnhandledExceptionLogger);
	LOGI("Nexus Ultimate starting...");
    {
        std::string keybindErr;
        if (!LoadKeybindsFromJson(&keybindErr))
            LOGW("Keybinds load: %s", keybindErr.c_str());
    }
    {
        std::string uiErr;
        if (!LoadUiSettingsFromJson(&uiErr))
            LOGW("UI settings load: %s", uiErr.c_str());
    }
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    const int kWinW = 920;
    const int kWinH = 520;

    GLFWwindow* window = glfwCreateWindow(kWinW, kWinH, "Nexus Ultimate", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    ApplyRoundedWindowRegion(window, 14);

    IMGUI_CHECKVERSION();
    ImGuiContext* imguiMainCtx = ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    constexpr int kScriptCfgW = 920;
    constexpr int kScriptCfgH = 620;
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* scriptCfgWindow = glfwCreateWindow(kScriptCfgW, kScriptCfgH, "Script configuration", nullptr, window);
    if (!scriptCfgWindow)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(imguiMainCtx);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    ImGuiContext* imguiScriptCfgCtx = ImGui::CreateContext();
    ImGui::SetCurrentContext(imguiScriptCfgCtx);
    ImGuiIO& ioScriptCfg = ImGui::GetIO();
    ioScriptCfg.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ioScriptCfg.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(scriptCfgWindow, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    ImGui::SetCurrentContext(imguiMainCtx);

    bool open = true;

    int lastW = kWinW, lastH = kWinH;
    int lastCfgW = kScriptCfgW, lastCfgH = kScriptCfgH;
    bool lastScriptCfgShown = false;
    g_running = true;
    std::thread worker(WorkerThread);
    auto lastSysCheck = std::chrono::steady_clock::now();
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int curW = 0, curH = 0;
        glfwGetWindowSize(window, &curW, &curH);
        if (curW != lastW || curH != lastH)
        {
            lastW = curW; lastH = curH;
            ApplyRoundedWindowRegion(window, 14);
        }

        if (g_scriptConfigWindowOpen)
        {
            int cw = 0, ch = 0;
            glfwGetWindowSize(scriptCfgWindow, &cw, &ch);
            if (cw != lastCfgW || ch != lastCfgH)
            {
                lastCfgW = cw; lastCfgH = ch;
                ApplyRoundedWindowRegion(scriptCfgWindow, 14);
            }
            if (!lastScriptCfgShown)
            {
                glfwShowWindow(scriptCfgWindow);
                glfwFocusWindow(scriptCfgWindow);
                ApplyRoundedWindowRegion(scriptCfgWindow, 14);
                lastScriptCfgShown = true;
            }
        }
        else if (lastScriptCfgShown)
        {
            glfwHideWindow(scriptCfgWindow);
            lastScriptCfgShown = false;
        }

        if (glfwWindowShouldClose(scriptCfgWindow))
        {
            glfwSetWindowShouldClose(scriptCfgWindow, GLFW_FALSE);
            g_scriptConfigWindowOpen = false;
        }

        ImGui::SetCurrentContext(imguiMainCtx);
        glfwMakeContextCurrent(window);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (open)
            RenderNexusUltimate(window, &open);
        ImGui::Render();

        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

        if (g_scriptConfigWindowOpen)
        {
            ImGui::SetCurrentContext(imguiScriptCfgCtx);
            glfwMakeContextCurrent(scriptCfgWindow);
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            NexusUiColors uiColors{};
            Nexus_GetThemeUiColors(&uiColors);
            RenderOperatorConfigWindow(scriptCfgWindow, &g_scriptConfigWindowOpen, &uiColors);

            ImGui::Render();

            int cfg_fb_w = 0, cfg_fb_h = 0;
            glfwGetFramebufferSize(scriptCfgWindow, &cfg_fb_w, &cfg_fb_h);
            glViewport(0, 0, cfg_fb_w, cfg_fb_h);
            glClearColor(0.06f, 0.06f, 0.07f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(scriptCfgWindow);

            ImGui::SetCurrentContext(imguiMainCtx);
            glfwMakeContextCurrent(window);
        }

        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSysCheck).count();

        if (ms >= 3000)
        {
            lastSysCheck = now;
            LogSystemTelemetry("3s");
        }
    }
    g_running = false;
    if (worker.joinable())
        worker.join();
    if (!SaveKeybindsToJson(nullptr))
        LOGW("Keybinds save failed");
    if (!SaveUiSettingsToJson(nullptr))
        LOGW("UI settings save failed");

    ImGui::SetCurrentContext(imguiScriptCfgCtx);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(imguiScriptCfgCtx);

    ImGui::SetCurrentContext(imguiMainCtx);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(imguiMainCtx);

    glfwDestroyWindow(scriptCfgWindow);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
