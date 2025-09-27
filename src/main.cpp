#include "http_server.hpp"
#include <iostream>

int main(int argc, char** argv) {
    std::uint16_t port = 8080;
    if (argc >= 2) { try { port = static_cast<std::uint16_t>(std::stoi(argv[1])); } catch (...) {} }

    HttpServer server(port);

    // GET /
    server.route("GET", "/", [](const HttpRequest& req, HttpResponse& res){
        res.headers["content-type"] = "text/plain; charset=utf-8";
        res.body = "It works! Try GET /hello or POST /echo\n";
    });

    // GET /hello
    server.route("GET", "/hello", [](const HttpRequest& req, HttpResponse& res){
        auto ua = req.headers.contains("user-agent") ? req.headers.at("user-agent") : "(unknown)";
        res.headers["content-type"] = "text/plain; charset=utf-8";
        res.body = std::string("Hello from C++!\nYour User-Agent: ") + ua + "\n";
    });

    // POST /echo
    server.route("POST", "/echo", [](const HttpRequest& req, HttpResponse& res){
        res.headers["content-type"] = "text/plain; charset=utf-8";
        res.body = req.body; // echo request body
    });

    server.run(); // blocking
    return 0;
}