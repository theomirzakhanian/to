#include "http.h"
#include "error.h"

#include <cstring>
#include <sstream>
#include <iostream>
#include <algorithm>

#ifndef __EMSCRIPTEN__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <csignal>
#include <thread>
#include <mutex>
#endif

// ========================
// HTTP Request Parsing
// ========================

static std::string urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int hex = std::stoi(str.substr(i + 1, 2), nullptr, 16);
            result += static_cast<char>(hex);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

static std::unordered_map<std::string, std::string> parseQueryString(const std::string& qs) {
    std::unordered_map<std::string, std::string> params;
    std::istringstream stream(qs);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            params[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
        } else {
            params[urlDecode(pair)] = "";
        }
    }
    return params;
}

HttpRequest HttpRequest::parse(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);
    std::string line;

    // Parse request line: GET /path HTTP/1.1
    if (std::getline(stream, line)) {
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream reqLine(line);
        reqLine >> req.method >> req.path >> req.httpVersion;
    }

    // Parse query params from path
    size_t qmark = req.path.find('?');
    if (qmark != std::string::npos) {
        std::string qs = req.path.substr(qmark + 1);
        req.path = req.path.substr(0, qmark);
        req.queryParams = parseQueryString(qs);
    }

    // Parse headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim leading whitespace from value
            size_t start = value.find_first_not_of(" \t");
            if (start != std::string::npos) value = value.substr(start);
            // Lowercase the key for easy lookup
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
            req.headers[lowerKey] = value;
        }
    }

    // Parse body (rest of the stream)
    std::string bodyPart;
    while (std::getline(stream, bodyPart)) {
        req.body += bodyPart;
        if (!stream.eof()) req.body += "\n";
    }

    return req;
}

ToValuePtr HttpRequest::toValue() const {
    std::vector<std::pair<std::string, ToValuePtr>> dict;
    dict.push_back({"method", ToValue::makeString(method)});
    dict.push_back({"path", ToValue::makeString(path)});

    // Headers as dict
    std::vector<std::pair<std::string, ToValuePtr>> headerDict;
    for (auto& [k, v] : headers) {
        headerDict.push_back({k, ToValue::makeString(v)});
    }
    dict.push_back({"headers", ToValue::makeDict(std::move(headerDict))});

    // Query params as dict
    std::vector<std::pair<std::string, ToValuePtr>> paramDict;
    for (auto& [k, v] : queryParams) {
        paramDict.push_back({k, ToValue::makeString(v)});
    }
    dict.push_back({"params", ToValue::makeDict(std::move(paramDict))});

    dict.push_back({"body", ToValue::makeString(body)});

    return ToValue::makeDict(std::move(dict));
}

// ========================
// HTTP Response Building
// ========================

HttpResponse HttpResponse::fromValue(ToValuePtr val) {
    HttpResponse resp;
    resp.status = 200;
    resp.body = "";

    if (val->type == ToValue::Type::STRING) {
        // Simple string response
        resp.body = val->strVal;
        resp.headers["Content-Type"] = "text/plain; charset=utf-8";
        return resp;
    }

    if (val->type == ToValue::Type::DICT) {
        for (auto& [k, v] : val->dictVal) {
            if (k == "status" && v->type == ToValue::Type::INT) {
                resp.status = (int)v->intVal;
            } else if (k == "body" && v->type == ToValue::Type::STRING) {
                resp.body = v->strVal;
            } else if (k == "body" && v->type == ToValue::Type::DICT) {
                // Auto-serialize dict body as JSON
                resp.body = ""; // will build JSON below
                resp.headers["Content-Type"] = "application/json; charset=utf-8";
                // Simple JSON serializer
                resp.body = "{";
                bool first = true;
                for (auto& [jk, jv] : v->dictVal) {
                    if (!first) resp.body += ", ";
                    resp.body += "\"" + jk + "\": ";
                    if (jv->type == ToValue::Type::STRING) {
                        resp.body += "\"" + jv->strVal + "\"";
                    } else {
                        resp.body += jv->toString();
                    }
                    first = false;
                }
                resp.body += "}";
            } else if (k == "body" && v->type == ToValue::Type::LIST) {
                resp.headers["Content-Type"] = "application/json; charset=utf-8";
                resp.body = "[";
                for (size_t i = 0; i < v->listVal.size(); i++) {
                    if (i > 0) resp.body += ", ";
                    auto& item = v->listVal[i];
                    if (item->type == ToValue::Type::STRING) {
                        resp.body += "\"" + item->strVal + "\"";
                    } else if (item->type == ToValue::Type::DICT) {
                        resp.body += "{";
                        bool first = true;
                        for (auto& [jk, jv] : item->dictVal) {
                            if (!first) resp.body += ", ";
                            resp.body += "\"" + jk + "\": ";
                            if (jv->type == ToValue::Type::STRING) resp.body += "\"" + jv->strVal + "\"";
                            else resp.body += jv->toString();
                            first = false;
                        }
                        resp.body += "}";
                    } else {
                        resp.body += item->toString();
                    }
                }
                resp.body += "]";
            } else if (k == "headers" && v->type == ToValue::Type::DICT) {
                for (auto& [hk, hv] : v->dictVal) {
                    resp.headers[hk] = hv->toString();
                }
            } else if (k == "type" && v->type == ToValue::Type::STRING) {
                resp.headers["Content-Type"] = v->strVal;
            }
        }
    }

    if (resp.headers.find("Content-Type") == resp.headers.end()) {
        resp.headers["Content-Type"] = "text/plain; charset=utf-8";
    }

    return resp;
}

std::string HttpResponse::serialize() const {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << " " << HttpServer::getStatusText(status) << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n";
    for (auto& [k, v] : headers) {
        out << k << ": " << v << "\r\n";
    }
    out << "\r\n";
    out << body;
    return out.str();
}

// ========================
// HTTP Server
// ========================

std::string HttpServer::getStatusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

#ifndef __EMSCRIPTEN__
void HttpServer::handleClient(int clientFd, Handler& handler) {
    // Read request
    char buffer[8192];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead <= 0) {
        close(clientFd);
        return;
    }

    std::string rawRequest(buffer, bytesRead);

    // If Content-Length indicates more data, read it
    auto req = HttpRequest::parse(rawRequest);
    auto it = req.headers.find("content-length");
    if (it != req.headers.end()) {
        size_t contentLength = std::stoul(it->second);
        // Find where body starts in raw request
        size_t headerEnd = rawRequest.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            size_t bodyStart = headerEnd + 4;
            size_t bodyReceived = rawRequest.size() - bodyStart;
            while (bodyReceived < contentLength) {
                memset(buffer, 0, sizeof(buffer));
                bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
                if (bytesRead <= 0) break;
                rawRequest.append(buffer, bytesRead);
                bodyReceived += bytesRead;
            }
            // Re-parse with full body
            req = HttpRequest::parse(rawRequest);
        }
    }

    // Convert request to To value
    auto reqVal = req.toValue();

    // Call handler
    HttpResponse resp;
    try {
        auto result = handler(reqVal);
        resp = HttpResponse::fromValue(result);
    } catch (std::exception& e) {
        resp.status = 500;
        resp.body = std::string("{\"error\": \"") + e.what() + "\"}";
        resp.headers["Content-Type"] = "application/json";
    }

    // Send response
    std::string respStr = resp.serialize();
    send(clientFd, respStr.c_str(), respStr.size(), 0);
    close(clientFd);

    // Log
    std::cout << req.method << " " << req.path << " -> " << resp.status << "\n";
}

void HttpServer::serve(int port, Handler handler) {
    defaultHandler = handler;

    // Create socket
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        throw ToRuntimeError("Failed to create socket");
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(serverFd);
        throw ToRuntimeError("Failed to bind to port " + std::to_string(port) + " (is it already in use?)");
    }

    // Listen
    if (listen(serverFd, 128) < 0) {
        close(serverFd);
        throw ToRuntimeError("Failed to listen on socket");
    }

    std::cout << "\n  To Web Server running on http://localhost:" << port << "\n";
    std::cout << "  Press Ctrl+C to stop.\n\n";

    // Ignore SIGPIPE (broken pipe on client disconnect)
    signal(SIGPIPE, SIG_IGN);

    // Multi-threaded accept loop
    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientFd < 0) {
            continue;
        }
        // Handle each request in its own thread
        std::thread([this, clientFd, handler]() mutable {
            handleClient(clientFd, handler);
        }).detach();
    }

    close(serverFd);
}

void HttpServer::addRoute(const std::string& method, const std::string& path, Handler handler) {
    routes.push_back({method, path, handler});
}

void HttpServer::serveRouted(int port) {
    auto routeHandler = [this](ToValuePtr reqVal) -> ToValuePtr {
        // Get method and path from request
        std::string method, path;
        for (auto& [k, v] : reqVal->dictVal) {
            if (k == "method") method = v->strVal;
            if (k == "path") path = v->strVal;
        }

        // Find matching route
        for (auto& [rMethod, rPath, rHandler] : routes) {
            if ((rMethod == "*" || rMethod == method) && rPath == path) {
                return rHandler(reqVal);
            }
        }

        // 404
        std::vector<std::pair<std::string, ToValuePtr>> resp;
        resp.push_back({"status", ToValue::makeInt(404)});
        resp.push_back({"body", ToValue::makeString("Not Found: " + path)});
        return ToValue::makeDict(std::move(resp));
    };

    serve(port, routeHandler);
}
#endif // __EMSCRIPTEN__

// ========================
// JSON utilities (available in all builds)
// ========================

std::string jsonStringify(ToValuePtr val) {
    switch (val->type) {
        case ToValue::Type::STRING: {
            std::string escaped;
            for (char c : val->strVal) {
                switch (c) {
                    case '"': escaped += "\\\""; break;
                    case '\\': escaped += "\\\\"; break;
                    case '\n': escaped += "\\n"; break;
                    case '\t': escaped += "\\t"; break;
                    case '\r': escaped += "\\r"; break;
                    default: escaped += c;
                }
            }
            return "\"" + escaped + "\"";
        }
        case ToValue::Type::INT: return std::to_string(val->intVal);
        case ToValue::Type::FLOAT: {
            std::ostringstream oss;
            oss << val->floatVal;
            return oss.str();
        }
        case ToValue::Type::BOOL: return val->boolVal ? "true" : "false";
        case ToValue::Type::NONE: return "null";
        case ToValue::Type::LIST: {
            std::string result = "[";
            for (size_t i = 0; i < val->listVal.size(); i++) {
                if (i > 0) result += ", ";
                result += jsonStringify(val->listVal[i]);
            }
            return result + "]";
        }
        case ToValue::Type::DICT: {
            std::string result = "{";
            bool first = true;
            for (auto& [k, v] : val->dictVal) {
                if (!first) result += ", ";
                result += "\"" + k + "\": " + jsonStringify(v);
                first = false;
            }
            return result + "}";
        }
        default: return "null";
    }
}

// ========================
// JsonParser method implementations
// ========================

char JsonParser::peek() { return pos < src.size() ? src[pos] : '\0'; }
char JsonParser::advance() { return pos < src.size() ? src[pos++] : '\0'; }
void JsonParser::skipWhitespace() { while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r')) pos++; }

ToValuePtr JsonParser::parse() {
    skipWhitespace();
    return parseValue();
}

ToValuePtr JsonParser::parseValue() {
    skipWhitespace();
    char c = peek();
    if (c == '"') return parseString();
    if (c == '{') return parseObject();
    if (c == '[') return parseArray();
    if (c == 't' || c == 'f') return parseBool();
    if (c == 'n') return parseNull();
    if (c == '-' || isdigit(c)) return parseNumber();
    throw ToRuntimeError("Invalid JSON at position " + std::to_string(pos));
}

ToValuePtr JsonParser::parseString() {
    advance(); // skip "
    std::string result;
    while (pos < src.size() && src[pos] != '"') {
        if (src[pos] == '\\') {
            pos++;
            switch (src[pos]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '/': result += '/'; break;
                default: result += src[pos]; break;
            }
        } else {
            result += src[pos];
        }
        pos++;
    }
    if (pos < src.size()) pos++; // skip closing "
    return ToValue::makeString(result);
}

ToValuePtr JsonParser::parseNumber() {
    size_t start = pos;
    bool isFloat = false;
    if (src[pos] == '-') pos++;
    while (pos < src.size() && isdigit(src[pos])) pos++;
    if (pos < src.size() && src[pos] == '.') { isFloat = true; pos++; while (pos < src.size() && isdigit(src[pos])) pos++; }
    if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) { isFloat = true; pos++; if (src[pos] == '+' || src[pos] == '-') pos++; while (pos < src.size() && isdigit(src[pos])) pos++; }
    std::string numStr = src.substr(start, pos - start);
    if (isFloat) return ToValue::makeFloat(std::stod(numStr));
    return ToValue::makeInt(std::stoll(numStr));
}

ToValuePtr JsonParser::parseObject() {
    advance(); // skip {
    std::vector<std::pair<std::string, ToValuePtr>> entries;
    skipWhitespace();
    if (peek() == '}') { advance(); return ToValue::makeDict(std::move(entries)); }
    while (true) {
        skipWhitespace();
        if (peek() != '"') throw ToRuntimeError("Expected string key in JSON object");
        auto key = parseString();
        skipWhitespace();
        if (advance() != ':') throw ToRuntimeError("Expected ':' in JSON object");
        auto val = parseValue();
        entries.push_back({key->strVal, val});
        skipWhitespace();
        if (peek() == '}') { advance(); break; }
        if (advance() != ',') throw ToRuntimeError("Expected ',' or '}' in JSON object");
    }
    return ToValue::makeDict(std::move(entries));
}

ToValuePtr JsonParser::parseArray() {
    advance(); // skip [
    std::vector<ToValuePtr> items;
    skipWhitespace();
    if (peek() == ']') { advance(); return ToValue::makeList(std::move(items)); }
    while (true) {
        items.push_back(parseValue());
        skipWhitespace();
        if (peek() == ']') { advance(); break; }
        if (advance() != ',') throw ToRuntimeError("Expected ',' or ']' in JSON array");
    }
    return ToValue::makeList(std::move(items));
}

ToValuePtr JsonParser::parseBool() {
    if (src.substr(pos, 4) == "true") { pos += 4; return ToValue::makeBool(true); }
    if (src.substr(pos, 5) == "false") { pos += 5; return ToValue::makeBool(false); }
    throw ToRuntimeError("Invalid JSON boolean");
}

ToValuePtr JsonParser::parseNull() {
    if (src.substr(pos, 4) == "null") { pos += 4; return ToValue::makeNone(); }
    throw ToRuntimeError("Invalid JSON null");
}
