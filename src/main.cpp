#include "core/Application.hpp"

#include <Windows.h>
#include <objbase.h>

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int showCommand) {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    smf::core::Application application;
    const int result = application.Run(instance, showCommand);

    if (SUCCEEDED(comResult)) {
        CoUninitialize();
    }

    return result;
}

