cmake_minimum_required(VERSION 3.20)

project(SideKeyWin7 VERSION 1.0.0 LANGUAGES CXX RC)

add_executable(sidekey WIN32
  src/main.cpp
  src/app.rc
)

target_compile_features(sidekey PRIVATE cxx_std_17)
target_compile_definitions(sidekey PRIVATE
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  _WIN32_WINNT=0x0601
  NTDDI_VERSION=0x06010000
)

target_include_directories(sidekey PRIVATE src)
target_link_libraries(sidekey PRIVATE user32 shell32 advapi32 gdi32 comctl32)

if(MSVC)
  set_property(TARGET sidekey PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(sidekey PRIVATE /W4 /permissive-)
  target_link_options(sidekey PRIVATE /SUBSYSTEM:WINDOWS,6.01)
else()
  target_compile_options(sidekey PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -mcrtdll=msvcrt
  )
  target_link_options(sidekey PRIVATE
    -mcrtdll=msvcrt
    -municode
    -static
    -static-libgcc
    -static-libstdc++
    -Wl,--major-subsystem-version,6,--minor-subsystem-version,1
  )
endif()
