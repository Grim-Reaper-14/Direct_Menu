#include "core/Application.hpp"

#include "resources/resource.h"
#include "ui/Menu.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <format>
#include <iostream>
#include <string>
#include <thread>

// Dear ImGui intentionally leaves this callback declaration to the application
// because including Windows types from imgui_impl_win32.h would pollute users of
// that header.
extern