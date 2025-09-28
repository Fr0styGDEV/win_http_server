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

Make sure these are installed and on your PATH:
```bash
g++ --version
cmake --version
ninja --version
