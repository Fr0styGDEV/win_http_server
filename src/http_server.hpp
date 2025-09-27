# pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

struct HttpRequest {
    std::string method;
    std::string target;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status = 200;
    std::string reason = "OK"; 
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

using NativeSocket = std::uintptr_t;

class HttpServer {
    public:
        using Handler = std::function<void(const HttpRequest&, HttpResponse&)>;

        explicit HttpServer(std::uint16_t port = 8080,
                            unsigned threads = std::thread::hardware_concurrency());
        ~HttpServer();

        void route(std::string method, std::string path, Handler handler);
        void run();
        void stop();

    private:
            std::uint16_t port_;
            std::atomic_bool running_{false};

            // listening socket
            NativeSocket listen_socket_ = 0;

            // tiny worker pool
            unsigned worker_count_;
            std::vector<std::thread> workers_;
            std::queue<NativeSocket> socket_queue_;
            std::mutex q_mtx_;
            std::condition_variable q_cv_;

            // simple router
        std::vector<std::pair<std::pair<std::string,std::string>, Handler>> routes_;
        std::mutex routes_mtx_;

        // core loops
        bool init_platform();
        void shutdown_platform();
        bool open_listen_socket();
        void close_listen_socket();

        void accept_loop();
        void worker_loop();

        // one connection lifecycle
        void handle_connection(NativeSocket s);

        // http helpers
        static bool parse_request_headers(const std::string& header_block, HttpRequest& out);
        static std::string build_response_bytes(const HttpResponse& res);

};
