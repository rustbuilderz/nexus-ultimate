#pragma once
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#define GLFW_INCLUDE_NONE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <objidl.h>
#include <propidl.h>

#include <wincodec.h>

#include <gdiplus.h>
#include <windowsx.h>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstdarg>
#include <nlohmann/json.hpp>
#include <string_view>
#include <cmath>
#include <dwmapi.h>

#include "config.h"
#include "console_alloc.h"
#include "globals.h"
#include "json_parsing/loadops.h"

#pragma comment(lib, "dwmapi.lib")
