#pragma once

#include "config.h"
#include "globals.h"

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#include <Windows.h>
#include <gdiplus.h>

namespace Console {

    enum class LogLevel { Trace, Info, Warn, Error, Fatal };

    inline std::mutex g_logMutex;

    inline std::atomic<bool> g_consoleVisible{ false };
    inline HWND g_consoleHwnd = nullptr;

    inline std::ofstream g_logFile;

    inline std::string NowTimeString() {
        using namespace std::chrono;

        const auto tp = system_clock::now();
        const auto t = system_clock::to_time_t(tp);

        std::tm tm{};
        localtime_s(&tm, &t);

        const auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S")
            << '.' << std::setw(3) << std::setfill('0') << ms.count();

        return oss.str();
    }

    inline const char* LevelTag(LogLevel lv) {
        switch (lv) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        }
        return "INFO ";
    }

    inline const char* LevelColor(LogLevel lv) {
        switch (lv) {
        case LogLevel::Trace: return "\x1b[90m";
        case LogLevel::Info:  return "\x1b[36m";
        case LogLevel::Warn:  return "\x1b[33m";
        case LogLevel::Error: return "\x1b[31m";
        case LogLevel::Fatal: return "\x1b[41;97m";
        }
        return "\x1b[0m";
    }

    inline void InitFileLogger() {
        std::lock_guard<std::mutex> lk(g_logMutex);

        if (!g_logFile.is_open()) {
            g_logFile.open("log.txt", std::ios::out | std::ios::app);
        }

        if (g_logFile.is_open()) {
            g_logFile << "\n===== LOG SESSION START: "
                << NowTimeString()
                << " =====\n";
            g_logFile.flush();
        }
    }

    inline void LogV(LogLevel lv, const char* fmt, va_list ap) {
        char buf[4096]{};
        vsnprintf_s(buf, _TRUNCATE, fmt, ap);

        const std::string time = NowTimeString();

        std::ostringstream line;
        line << '[' << time << "] "
            << '[' << LevelTag(lv) << "] "
            << buf;

        std::lock_guard<std::mutex> lk(g_logMutex);

        std::cout << LevelColor(lv)
            << line.str()
            << "\x1b[0m\n";
        std::cout.flush();

        if (g_logFile.is_open()) {
            g_logFile << line.str() << '\n';
            g_logFile.flush();
        }
    }

    inline void Log(LogLevel lv, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        LogV(lv, fmt, ap);
        va_end(ap);
    }

    inline std::wstring Win32ErrorMessageW(DWORD err) {
        wchar_t* buf = nullptr;

        const DWORD flags =
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS;

        const DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);

        const DWORD n = FormatMessageW(
            flags, nullptr, err, lang,
            reinterpret_cast<LPWSTR>(&buf),
            0, nullptr);

        std::wstring msg = (n && buf) ? std::wstring(buf, buf + n) : L"(no message)";
        if (buf) LocalFree(buf);

        while (!msg.empty()) {
            wchar_t c = msg.back();
            if (c != L'\r' && c != L'\n' && c != L' ' && c != L'\t') break;
            msg.pop_back();
        }

        return msg;
    }

    inline bool EnableVirtualTerminal() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return false;

        DWORD mode = 0;
        if (!GetConsoleMode(hOut, &mode)) return false;

        mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        return SetConsoleMode(hOut, mode) != 0;
    }

    inline void AllocateConsoleOnce() {
        static std::atomic<bool> done{ false };
        bool expected = false;
        if (!done.compare_exchange_strong(expected, true)) return;

        if (!AllocConsole()) {
            AttachConsole(ATTACH_PARENT_PROCESS);
        }

        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        freopen_s(&f, "CONIN$", "r", stdin);

        std::ios::sync_with_stdio(true);

        EnableVirtualTerminal();
        SetConsoleTitleW(L"Debug Console");

        InitFileLogger();

        g_consoleHwnd = GetConsoleWindow();
        g_consoleVisible = (g_consoleHwnd != nullptr);

        Log(LogLevel::Info, "Console initialized. hwnd=%p", (void*)g_consoleHwnd);
    }

    inline void ToggleConsole() {
        if (!g_consoleHwnd) g_consoleHwnd = GetConsoleWindow();
        if (!g_consoleHwnd) {
            Log(LogLevel::Warn, "ToggleConsole failed: no hwnd");
            return;
        }

        const bool vis = g_consoleVisible.load();
        ShowWindow(g_consoleHwnd, vis ? SW_HIDE : SW_SHOW);
        g_consoleVisible = !vis;

        if (!vis) SetForegroundWindow(g_consoleHwnd);

        Log(LogLevel::Info, "Console %s", g_consoleVisible ? "SHOWN" : "HIDDEN");
    }

    inline LONG WINAPI UnhandledExceptionLogger(EXCEPTION_POINTERS* ep) {
        const DWORD code = ep ? ep->ExceptionRecord->ExceptionCode : 0;
        void* addr = ep ? ep->ExceptionRecord->ExceptionAddress : nullptr;

        Log(LogLevel::Fatal,
            "Unhandled exception: code=0x%08lX addr=%p",
            (unsigned long)code, addr);

        if (g_logFile.is_open()) {
            g_logFile << "===== CRASH =====\n";
            g_logFile.flush();
        }

        return EXCEPTION_EXECUTE_HANDLER;
    }

}

#define LOGT(...) ::Console::Log(::Console::LogLevel::Trace, __VA_ARGS__)
#define LOGI(...) ::Console::Log(::Console::LogLevel::Info,  __VA_ARGS__)
#define LOGW(...) ::Console::Log(::Console::LogLevel::Warn,  __VA_ARGS__)
#define LOGE(...) ::Console::Log(::Console::LogLevel::Error, __VA_ARGS__)
#define LOGF(...) ::Console::Log(::Console::LogLevel::Fatal, __VA_ARGS__)

#define WIN_CHECK(expr) do { \
    if (!(expr)) { \
        const DWORD _e = GetLastError(); \
        const std::wstring _m = ::Console::Win32ErrorMessageW(_e); \
        std::string _m8(_m.begin(), _m.end()); \
        LOGE("WIN_CHECK failed: %s err=%lu %s (%s:%d)", \
            #expr, (unsigned long)_e, _m8.c_str(), __FILE__, __LINE__); \
    } \
} while (0)

#define WIN_CHECK_RET(expr, ret) do { \
    if (!(expr)) { \
        const DWORD _e = GetLastError(); \
        const std::wstring _m = ::Console::Win32ErrorMessageW(_e); \
        std::string _m8(_m.begin(), _m.end()); \
        LOGE("WIN_CHECK failed: %s err=%lu %s (%s:%d)", \
            #expr, (unsigned long)_e, _m8.c_str(), __FILE__, __LINE__); \
        return (ret); \
    } \
} while (0)

inline const char* GdiplusStatusStr(Gdiplus::Status s) {
    using namespace Gdiplus;
    switch (s) {
    case Ok: return "Ok";
    case GenericError: return "GenericError";
    case InvalidParameter: return "InvalidParameter";
    case OutOfMemory: return "OutOfMemory";
    case ObjectBusy: return "ObjectBusy";
    case InsufficientBuffer: return "InsufficientBuffer";
    case NotImplemented: return "NotImplemented";
    case Win32Error: return "Win32Error";
    case WrongState: return "WrongState";
    case Aborted: return "Aborted";
    case FileNotFound: return "FileNotFound";
    case ValueOverflow: return "ValueOverflow";
    case AccessDenied: return "AccessDenied";
    case UnknownImageFormat: return "UnknownImageFormat";
    case FontFamilyNotFound: return "FontFamilyNotFound";
    case FontStyleNotFound: return "FontStyleNotFound";
    case NotTrueTypeFont: return "NotTrueTypeFont";
    case UnsupportedGdiplusVersion: return "UnsupportedGdiplusVersion";
    case GdiplusNotInitialized: return "GdiplusNotInitialized";
    case PropertyNotFound: return "PropertyNotFound";
    case PropertyNotSupported: return "PropertyNotSupported";
    default: return "Unknown";
    }
}

#define GDIP_CHECK_OK(expr) do { \
    const auto _s = (expr); \
    if (_s != Gdiplus::Ok) { \
        LOGE("GDI+ failed: %s -> %s (%s:%d)", \
            #expr, GdiplusStatusStr(_s), __FILE__, __LINE__); \
    } \
} while (0)