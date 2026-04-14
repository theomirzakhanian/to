#pragma once
#include "environment.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string httpVersion;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::unordered_map<std::string, std::string> queryParams;

    // Parse raw HTTP request
    static HttpRequest parse(const std::string& raw);

    // Convert to a To dict value
    ToValuePtr toValue() const;
};

struct HttpResponse {
    int status = 200;
    std::string statusText = "OK";
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    // Build from a To dict value
    static HttpResponse fromValue(ToValuePtr val);

    // Serialize to HTTP response string
    std::string serialize() const;
};

// The actual TCP server
class HttpServer {
public:
    using Handler = std::function<ToValuePtr(ToValuePtr)>;

    HttpServer() = default;

    // Start listening and serving
    void serve(int port, Handler handler);

    // Route-based serving
    void addRoute(const std::string& method, const std::string& path, Handler handler);
    void serveRouted(int port);

    static std::string getStatusText(int code);

private:
    std::vector<std::tuple<std::string, std::string, Handler>> routes;
    Handler defaultHandler;

    void handleClient(int clientFd, Handler& handler);
};

// JSON utilities
std::string jsonStringify(ToValuePtr val);

class JsonParser {
public:
    explicit JsonParser(const std::string& input) : src(input), pos(0) {}
    ToValuePtr parse();
private:
    std::string src;
    size_t pos;
    char peek();
    char advance();
    void skipWhitespace();
    ToValuePtr parseValue();
    ToValuePtr parseString();
    ToValuePtr parseNumber();
    ToValuePtr parseObject();
    ToValuePtr parseArray();
    ToValuePtr parseBool();
    ToValuePtr parseNull();
};
