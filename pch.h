#pragma once
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
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
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string_view>
#include <cmath>
#include <algorithm>
#include <dwmapi.h>

#include "rendering/bitmap_handler.h"
#include "config.h"
#include "console_alloc.h"
#include "globals.h"
#include "json_parsing/loadops.h"

#pragma comment(lib, "dwmapi.lib")
