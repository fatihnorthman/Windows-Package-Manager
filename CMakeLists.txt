cmake_minimum_required(VERSION 3.20)
project(WindowsPackageManager VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

add_executable(PackageManager WIN32
    src/main.cpp
    src/core/Logger.cpp
    src/core/ProcessRunner.cpp
    src/adapters/AdapterUtils.cpp
    src/adapters/WingetAdapter.cpp
    src/adapters/ScoopAdapter.cpp
    src/adapters/ChocoAdapter.cpp
    src/services/TaskQueue.cpp
    src/gui/i18n.cpp
    src/gui/BackendBridge.cpp
    src/gui/win32/Application.cpp
    src/gui/win32/Renderer.cpp
    src/gui/win32/Sidebar.cpp
    src/gui/win32/TopBar.cpp
    src/gui/win32/Footer.cpp
    src/gui/win32/Screens.cpp
)
target_include_directories(PackageManager PRIVATE src)
if(WIN32)
    target_link_libraries(PackageManager PRIVATE
        kernel32 user32 advapi32 shell32 ole32 oleaut32
        d2d1 d3d11 d3dcompiler dxgi dxguid dwmapi dwrite gdi32
    )
    set_target_properties(PackageManager PROPERTIES
        WIN32_EXECUTABLE ON
    )
endif()

install(TARGETS PackageManager RUNTIME DESTINATION bin)
