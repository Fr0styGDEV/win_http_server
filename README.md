# win_http_server

A minimal HTTP/1.1 server written in modern C++20 for Windows (using Winsock).  
Designed as a reusable starting point for future projects (like file transfer apps).

---

## Features
- Simple request/response API (`HttpRequest`, `HttpResponse`)
- Route registration: `server.route("GET", "/path", handler)`
- Thread pool for concurrent connections
- Basic HTTP parsing (request line, headers, `Content-Length`)
- Demo routes:
  - `GET /` → returns "It works!"
  - `GET /hello` → returns greeting with User-Agent
  - `POST /echo` → echoes the request body

---

## Requirements
- [MSYS2](https://www.msys2.org/) with MinGW-w64 toolchain
- CMake ≥ 3.20
- GCC ≥ 13 (C++20 support)

Verify you have the tools installed and on PATH:

    g++ --version
    cmake --version
    ninja --version

---

## Building

### Command line (MinGW + Ninja)

    # From project root
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build

### VS Code
- Install the **CMake Tools** extension.
- Open this folder in VS Code.
- Select Kit → `GCC x86_64-w64-mingw32` (from MSYS2 MinGW).
- Select Generator → `Ninja`.
- Hit **Build**.

---

## Running
Run the server (defaults to port 8080):

    ./build/win_http_server.exe

Test endpoints:

    curl http://127.0.0.1:8080/
    curl http://127.0.0.1:8080/hello
    curl.exe -X POST http://127.0.0.1:8080/echo -d "ping"   # on Windows PowerShell

---

## Usage Example
A simple app using the server:

    #include "http_server.hpp"

    int main() {
        HttpServer server(8080);

        server.route("GET", "/", [](const HttpRequest& req, HttpResponse& res){
            res.headers["content-type"] = "text/plain";
            res.body = "Hello from win_http_server!";
        });

        server.run();
    }

---

## Project structure

    win_http_server/
    ├─ CMakeLists.txt
    └─ src/
       ├─ main.cpp
       ├─ http_server.hpp
       └─ http_server.cpp

---
