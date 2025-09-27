cmake_minimum_required(VERSION 3.20)
project(win_http_server CXX)

add_executable(win_http_server
    src/main.cpp
    src/http_server.cpp
)

target_compile_features(win_http_server PRIVATE cxx_std_20)
target_compile_definitions(win_http_server PRIVATE _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)

target_include_directories(win_http_server PRIVATE src)
target_link_libraries(win_http_server PRIVATE ws2_32)
