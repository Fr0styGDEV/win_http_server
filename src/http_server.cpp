#include "http_server.hpp"
#include <iostream> 
#include <sstream>
#include <algorithm>
#include <charconv>

#ifdef _WIN32
  #define NOMINMAX
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
#endif

// local helpers
namespace {
    std::string to_lower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c)); });
        return s;
    }

    std::string_view trim(std::string_view v) {
        size_t a = 0, b = v.size();
        while (a < b && std::isspace(static_cast<unsigned char>(v[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(v[b-1]))) --b;
        return v.substr(a, b - a);
    }
}

// map NativeSocket to Winsock SOCKET
static SOCKET to_win_socket(NativeSocket s) {
    return reinterpret_cast<SOCKET>(s);
}
static NativeSocket from_win_socket(SOCKET s){
    return reinterpret_cast<NativeSocket>(s);
}
// contructor
HttpServer::HttpServer(std::uint16_t port, unsigned threads) 
    : port_(port),
      worker_count_(threads == 0 ? 4u : threads) {}
// destructor
HttpServer::~HttpServer() {
    stop();
    for (auto& t: workers_)
        if (t.joinable()) t.join();
    shutdown_platform();
}

// platform init/cleanup
bool HttpServer::init_platform() {
#ifdef _WIN32
    WSADATA wsa{};
    int r = WSAStartup(MAKEWORD(2,2), &wsa);
    if (r != 0) {
        std::cerr << "WSAStartup failed: " << r << "/n";
        return false;
    }
#endif
    return true;
}

void HttpServer::shutdown_platform() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// open/close listening socket
// bind to port and start listening
bool HttpServer::open_listen_socket() {
#ifdef _WIN32
    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags    = AI_PASSIVE;

    addrinfo* res = nullptr;
    std::string port_str = std::to_string(port_);
    if (int r = getaddrinfo(nullptr, port_str.c_str(), &hints, &res); r != 0) {
        std::cerr << "getaddrinfo failed: " << r << "\n";
        return false;
    }

    SOCKET ls = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (ls == INVALID_SOCKET) {
        std::cerr << "socket() failed: " << WSAGetLastError() << "\n";
        freeaddrinfo(res);
        return false;
    }

    int opt = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    if (bind(ls, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        std::cerr << "bind() failed: " << WSAGetLastError() << "\n";
        freeaddrinfo(res);
        closesocket(ls);
        return false;
    }
    freeaddrinfo(res);

    if (listen(ls, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed: " << WSAGetLastError() << "\n";
        closesocket(ls);
        return false;
    }

    listen_socket_ = from_win_socket(ls);
    return true;
#else
    return false;
#endif
}

void HttpServer::close_listen_socket() {
#ifdef _WIN32
    SOCKET ls = to_win_socket(listen_socket_);
    if (ls != INVALID_SOCKET) {
        closesocket(ls);
        listen_socket_ = 0;
    }
#endif
}

// run() and stop()
void HttpServer::run() {
    if (!init_platform()) return;
    if (!open_listen_socket()) return;

    running_ = true;

    for (unsigned i = 0; i < worker_count_; ++i) {
        workers_.emplace_back(&HttpServer::worker_loop, this);
    }

    std::cout << "HTTP server listening on http://127.0.0.1:" << port_
              << "  (Ctrl+C to stop)\n";

    accept_loop();
}

void HttpServer::stop() {
    bool expected = true;
    if (running_.compare_exchange_strong(expected, false)) {
        close_listen_socket();
        q_cv_.notify_all();
    } else {
        // already stopped or never started
    }
}

// accept loop
void HttpServer::accept_loop() {
#ifdef _WIN32
    SOCKET ls = to_win_socket(listen_socket_);
    while (running_) {
        SOCKET client = accept(ls, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (!running_) break;
            int err = WSAGetLastError();
            if (err == WSAEINTR) continue;
            std::cerr << "accept() failed: " << err << "\n";
            continue;
        }
        {
            std::lock_guard lk(q_mtx_);
            socket_queue_.push(from_win_socket(client));
        }
        q_cv_.notify_one();
    }
#endif
}

// worker loop
void HttpServer::worker_loop() {
#ifdef _WIN32
    while (running_ || !socket_queue_.empty()) {
        NativeSocket ns = 0;
        {
            std::unique_lock lk(q_mtx_);
            q_cv_.wait(lk, [&]{ return !running_ || !socket_queue_.empty(); });
            if (!socket_queue_.empty()) {
                ns = socket_queue_.front();
                socket_queue_.pop();
            }
        }
        if (ns) {
            SOCKET s = to_win_socket(ns);
            handle_connection(ns);
            closesocket(s);
        }
    }
#endif
}

// Parse request line & headers
bool HttpServer::parse_request_headers(const std::string& header_block, HttpRequest& out) {
    // Find first CRLF (end of request line)
    size_t line_end = header_block.find("\r\n");
    if (line_end == std::string::npos) return false;

    // Request line: METHOD SP TARGET SP VERSION
    {
        std::istringstream rl(header_block.substr(0, line_end));
        if (!(rl >> out.method >> out.target >> out.version)) return false;
    }

    // Headers
    size_t pos = line_end + 2;
    while (pos < header_block.size()) {
        size_t e = header_block.find("\r\n", pos);
        if (e == std::string::npos) break;
        if (e == pos) { pos += 2; break; }
        std::string line = header_block.substr(pos, e - pos);
        pos = e + 2;

        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            key = to_lower(key);
            out.headers[key] = std::string(trim(val));
        }
    }
    return true;
}

// serialize a response
std::string HttpServer::build_response_bytes(const HttpResponse& res) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << res.status << ' '
        << (res.reason.empty() ? "OK" : res.reason) << "\r\n";

    std::unordered_map<std::string, std::string> h = res.headers;
    h.emplace("connection", "close");
    if (h.find("content-type") == h.end())
        h["content-type"] = "text/plain; charset=utf-8";
    h["content-length"] = std::to_string(res.body.size());

    for (auto& [k,v] : h) {
        oss << k << ": " << v << "\r\n";
    }
    oss << "\r\n" << res.body;
    return oss.str();
}

// handle single connection
void HttpServer::handle_connection(NativeSocket ns) {
#ifdef _WIN32
    SOCKET s = to_win_socket(ns);

    std::string buf; buf.reserve(4096);
    char tmp[4096];

    size_t header_end = std::string::npos;
    while (true) {
        int r = recv(s, tmp, sizeof(tmp), 0);
        if (r <= 0) return;
        buf.append(tmp, tmp + r);
        header_end = buf.find("\r\n\r\n");
        if (header_end != std::string::npos) break;
        if (buf.size() > 65536) return;
    }

    HttpRequest req;
    std::string header_block = buf.substr(0, header_end);
    if (!parse_request_headers(header_block, req)) {
        HttpResponse bad{400, "Bad Request", {}, "Malformed request\n"};
        auto bytes = build_response_bytes(bad);
        send(s, bytes.c_str(), (int)bytes.size(), 0);
        return;
    }

    size_t body_start = header_end + 4;
    size_t already = (buf.size() > body_start) ? (buf.size() - body_start) : 0;
    size_t want = 0;
    if (auto it = req.headers.find("content-length"); it != req.headers.end()) {
        want = static_cast<size_t>(std::stoul(it->second));
    }

    req.body.reserve(want);
    if (already) req.body.append(buf.data() + body_start, already);
    while (req.body.size() < want) {
        int r = recv(s, tmp, sizeof(tmp), 0);
        if (r <= 0) break;
        size_t need = want - req.body.size();
        req.body.append(tmp, tmp + std::min<size_t>(r, need));
    }

    HttpResponse res;
    HttpServer::Handler h = nullptr;
    {
        std::lock_guard lk(routes_mtx_);
        for (auto& entry : routes_) {
            auto& key = entry.first;
            if (key.first == req.method && key.second == req.target) {
                h = entry.second;
                break;
            }
        }
    }

    if (h) {
        try { h(req, res); }
        catch (const std::exception& e) {
            res.status = 500; res.reason = "Internal Server Error";
            res.body = std::string("Exception: ") + e.what() + "\n";
        }
    } else {
        res.status = 404; res.reason = "Not Found";
        res.headers["content-type"] = "text/plain; charset=utf-8";
        res.body = "Route not found\n";
    }

    auto bytes = build_response_bytes(res);
    size_t sent = 0;
    while (sent < bytes.size()) {
        int r = send(s, bytes.c_str() + sent, (int)(bytes.size() - sent), 0);
        if (r <= 0) break;
        sent += (size_t)r;
    }
#endif
}

// implemet route()
void HttpServer::route(std::string method, std::string path, Handler handler) {
    std::lock_guard lk(routes_mtx_);
    routes_.push_back({ {std::move(method), std::move(path)}, std::move(handler) });
}
