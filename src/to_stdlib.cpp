#include "to_stdlib.h"
#include "error.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <regex>
#include <fstream>
#include <thread>
#include <unistd.h>
#include <cstdio>

namespace fs = std::filesystem;

// ========================
// Time Module
// ========================

void registerTimeModule(EnvPtr env) {
    auto mod = ToValue::makeDict({});

    // time.now() — current unix timestamp as float (seconds)
    mod->dictVal.push_back({"now", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            auto now = std::chrono::system_clock::now();
            auto dur = now.time_since_epoch();
            double secs = std::chrono::duration<double>(dur).count();
            return ToValue::makeFloat(secs);
        }
    )});

    // time.ms() — current time in milliseconds
    mod->dictVal.push_back({"ms", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            auto now = std::chrono::system_clock::now();
            auto dur = now.time_since_epoch();
            int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
            return ToValue::makeInt(ms);
        }
    )});

    // time.sleep(seconds)
    mod->dictVal.push_back({"sleep", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1)
                throw ToRuntimeError("time.sleep() takes 1 argument");
            double secs;
            if (args[0]->type == ToValue::Type::INT) secs = (double)args[0]->intVal;
            else if (args[0]->type == ToValue::Type::FLOAT) secs = args[0]->floatVal;
            else throw ToRuntimeError("time.sleep() requires a number");
            std::this_thread::sleep_for(std::chrono::milliseconds((int)(secs * 1000)));
            return ToValue::makeNone();
        }
    )});

    // time.format(timestamp, format_string) — format a timestamp
    mod->dictVal.push_back({"format", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            double timestamp;
            std::string fmt = "%Y-%m-%d %H:%M:%S";
            if (args.size() >= 1) {
                if (args[0]->type == ToValue::Type::INT) timestamp = (double)args[0]->intVal;
                else if (args[0]->type == ToValue::Type::FLOAT) timestamp = args[0]->floatVal;
                else throw ToRuntimeError("time.format() first arg must be a number");
            } else {
                auto now = std::chrono::system_clock::now();
                timestamp = std::chrono::duration<double>(now.time_since_epoch()).count();
            }
            if (args.size() >= 2 && args[1]->type == ToValue::Type::STRING) {
                fmt = args[1]->strVal;
            }
            time_t t = (time_t)timestamp;
            struct tm* tm = localtime(&t);
            char buf[256];
            strftime(buf, sizeof(buf), fmt.c_str(), tm);
            return ToValue::makeString(buf);
        }
    )});

    // time.date() — current date as "YYYY-MM-DD"
    mod->dictVal.push_back({"date", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            auto now = std::chrono::system_clock::now();
            time_t t = std::chrono::system_clock::to_time_t(now);
            struct tm* tm = localtime(&t);
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
            return ToValue::makeString(buf);
        }
    )});

    // time.clock() — current time as "HH:MM:SS"
    mod->dictVal.push_back({"clock", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            auto now = std::chrono::system_clock::now();
            time_t t = std::chrono::system_clock::to_time_t(now);
            struct tm* tm = localtime(&t);
            char buf[64];
            strftime(buf, sizeof(buf), "%H:%M:%S", tm);
            return ToValue::makeString(buf);
        }
    )});

    // time.year(), time.month(), time.day(), time.hour(), time.minute(), time.second()
    mod->dictVal.push_back({"year", ToValue::makeBuiltin([](std::vector<ToValuePtr>) -> ToValuePtr {
        time_t t = time(nullptr); return ToValue::makeInt(localtime(&t)->tm_year + 1900);
    })});
    mod->dictVal.push_back({"month", ToValue::makeBuiltin([](std::vector<ToValuePtr>) -> ToValuePtr {
        time_t t = time(nullptr); return ToValue::makeInt(localtime(&t)->tm_mon + 1);
    })});
    mod->dictVal.push_back({"day", ToValue::makeBuiltin([](std::vector<ToValuePtr>) -> ToValuePtr {
        time_t t = time(nullptr); return ToValue::makeInt(localtime(&t)->tm_mday);
    })});
    mod->dictVal.push_back({"hour", ToValue::makeBuiltin([](std::vector<ToValuePtr>) -> ToValuePtr {
        time_t t = time(nullptr); return ToValue::makeInt(localtime(&t)->tm_hour);
    })});
    mod->dictVal.push_back({"minute", ToValue::makeBuiltin([](std::vector<ToValuePtr>) -> ToValuePtr {
        time_t t = time(nullptr); return ToValue::makeInt(localtime(&t)->tm_min);
    })});
    mod->dictVal.push_back({"second", ToValue::makeBuiltin([](std::vector<ToValuePtr>) -> ToValuePtr {
        time_t t = time(nullptr); return ToValue::makeInt(localtime(&t)->tm_sec);
    })});

    env->define("time", mod);
}

// ========================
// Filesystem Module
// ========================

void registerFSModule(EnvPtr env) {
    auto mod = ToValue::makeDict({});

    // fs.read(path) — read entire file as string
    mod->dictVal.push_back({"read", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("fs.read() takes 1 string argument");
            std::ifstream f(args[0]->strVal);
            if (!f.is_open()) throw ToRuntimeError("Cannot open file: " + args[0]->strVal);
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            return ToValue::makeString(content);
        }
    )});

    // fs.write(path, content) — write string to file
    mod->dictVal.push_back({"write", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 2 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("fs.write() takes 2 arguments (path, content)");
            std::ofstream f(args[0]->strVal);
            if (!f.is_open()) throw ToRuntimeError("Cannot open file for writing: " + args[0]->strVal);
            f << args[1]->toString();
            return ToValue::makeNone();
        }
    )});

    // fs.append(path, content) — append to file
    mod->dictVal.push_back({"append", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 2 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("fs.append() takes 2 arguments (path, content)");
            std::ofstream f(args[0]->strVal, std::ios::app);
            if (!f.is_open()) throw ToRuntimeError("Cannot open file: " + args[0]->strVal);
            f << args[1]->toString();
            return ToValue::makeNone();
        }
    )});

    // fs.exists(path)
    mod->dictVal.push_back({"exists", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("fs.exists() takes 1 string argument");
            return ToValue::makeBool(fs::exists(args[0]->strVal));
        }
    )});

    // fs.is_file(path)
    mod->dictVal.push_back({"is_file", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("fs.is_file() takes 1 string argument");
            return ToValue::makeBool(fs::is_regular_file(args[0]->strVal));
        }
    )});

    // fs.is_dir(path)
    mod->dictVal.push_back({"is_dir", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("fs.is_dir() takes 1 string argument");
            return ToValue::makeBool(fs::is_directory(args[0]->strVal));
        }
    )});

    // fs.list(path) — list directory contents
    mod->dictVal.push_back({"list", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            std::string path = ".";
            if (!args.empty() && args[0]->type == ToValue::Type::STRING)
                path = args[0]->strVal;
            std::vector<ToValuePtr> items;
            for (auto& entry : fs::directory_iterator(path)) {
                items.push_back(ToValue::makeString(entry.path().filename().string()));
            }
            return ToValue::makeList(std::move(items));
        }
    )});

    // fs.walk(path) — recursively list all files
    mod->dictVal.push_back({"walk", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            std::string path = ".";
            if (!args.empty() && args[0]->type == ToValue::Type::STRING)
                path = args[0]->strVal;
            std::vector<ToValuePtr> items;
            for (auto& entry : fs::recursive_directory_iterator(path)) {
                items.push_back(ToValue::makeString(entry.path().string()));
            }
            return ToValue::makeList(std::move(items));
        }
    )});

    // fs.mkdir(path)
    mod->dictVal.push_back({"mkdir", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("fs.mkdir() takes 1 string argument");
            fs::create_directories(args[0]->strVal);
            return ToValue::makeNone();
        }
    )});

    // fs.remove(path)
    mod->dictVal.push_back({"remove", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("fs.remove() takes 1 string argument");
            return ToValue::makeBool(fs::remove(args[0]->strVal));
        }
    )});

    // fs.copy(src, dest)
    mod->dictVal.push_back({"copy", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 2) throw ToRuntimeError("fs.copy() takes 2 arguments");
            fs::copy(args[0]->strVal, args[1]->strVal, fs::copy_options::overwrite_existing);
            return ToValue::makeNone();
        }
    )});

    // fs.rename(old, new)
    mod->dictVal.push_back({"rename", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 2) throw ToRuntimeError("fs.rename() takes 2 arguments");
            fs::rename(args[0]->strVal, args[1]->strVal);
            return ToValue::makeNone();
        }
    )});

    // fs.size(path) — file size in bytes
    mod->dictVal.push_back({"size", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("fs.size() takes 1 string argument");
            return ToValue::makeInt((int64_t)fs::file_size(args[0]->strVal));
        }
    )});

    // fs.ext(path) — get file extension
    mod->dictVal.push_back({"ext", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("fs.ext() takes 1 string argument");
            return ToValue::makeString(fs::path(args[0]->strVal).extension().string());
        }
    )});

    // fs.basename(path)
    mod->dictVal.push_back({"basename", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("fs.basename() takes 1 string argument");
            return ToValue::makeString(fs::path(args[0]->strVal).filename().string());
        }
    )});

    // fs.dirname(path)
    mod->dictVal.push_back({"dirname", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 1 || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("fs.dirname() takes 1 string argument");
            return ToValue::makeString(fs::path(args[0]->strVal).parent_path().string());
        }
    )});

    // fs.cwd() — current working directory
    mod->dictVal.push_back({"cwd", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            return ToValue::makeString(fs::current_path().string());
        }
    )});

    env->define("fs", mod);
}

// ========================
// Regex Module
// ========================

void registerRegexModule(EnvPtr env) {
    auto mod = ToValue::makeDict({});

    // regex.match(pattern, string) — returns first match or none
    mod->dictVal.push_back({"match", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 2 || args[0]->type != ToValue::Type::STRING || args[1]->type != ToValue::Type::STRING)
                throw ToRuntimeError("regex.match() takes 2 string arguments (pattern, text)");
            try {
                std::regex re(args[0]->strVal);
                std::smatch m;
                if (std::regex_search(args[1]->strVal, m, re)) {
                    std::vector<ToValuePtr> groups;
                    for (size_t i = 0; i < m.size(); i++) {
                        groups.push_back(ToValue::makeString(m[i].str()));
                    }
                    return ToValue::makeList(std::move(groups));
                }
                return ToValue::makeNone();
            } catch (std::regex_error& e) {
                throw ToRuntimeError("Invalid regex: " + std::string(e.what()));
            }
        }
    )});

    // regex.find_all(pattern, string) — returns all matches
    mod->dictVal.push_back({"find_all", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 2 || args[0]->type != ToValue::Type::STRING || args[1]->type != ToValue::Type::STRING)
                throw ToRuntimeError("regex.find_all() takes 2 string arguments");
            try {
                std::regex re(args[0]->strVal);
                std::string text = args[1]->strVal;
                std::vector<ToValuePtr> matches;
                auto begin = std::sregex_iterator(text.begin(), text.end(), re);
                auto end = std::sregex_iterator();
                for (auto it = begin; it != end; ++it) {
                    matches.push_back(ToValue::makeString((*it)[0].str()));
                }
                return ToValue::makeList(std::move(matches));
            } catch (std::regex_error& e) {
                throw ToRuntimeError("Invalid regex: " + std::string(e.what()));
            }
        }
    )});

    // regex.replace(pattern, replacement, string)
    mod->dictVal.push_back({"replace", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 3)
                throw ToRuntimeError("regex.replace() takes 3 arguments (pattern, replacement, text)");
            try {
                std::regex re(args[0]->strVal);
                return ToValue::makeString(std::regex_replace(args[2]->strVal, re, args[1]->strVal));
            } catch (std::regex_error& e) {
                throw ToRuntimeError("Invalid regex: " + std::string(e.what()));
            }
        }
    )});

    // regex.split(pattern, string)
    mod->dictVal.push_back({"split", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 2)
                throw ToRuntimeError("regex.split() takes 2 arguments (pattern, text)");
            try {
                std::regex re(args[0]->strVal);
                std::string text = args[1]->strVal;
                std::vector<ToValuePtr> parts;
                auto begin = std::sregex_token_iterator(text.begin(), text.end(), re, -1);
                auto end = std::sregex_token_iterator();
                for (auto it = begin; it != end; ++it) {
                    parts.push_back(ToValue::makeString(it->str()));
                }
                return ToValue::makeList(std::move(parts));
            } catch (std::regex_error& e) {
                throw ToRuntimeError("Invalid regex: " + std::string(e.what()));
            }
        }
    )});

    // regex.test(pattern, string) — returns bool
    mod->dictVal.push_back({"test", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() != 2)
                throw ToRuntimeError("regex.test() takes 2 arguments");
            try {
                std::regex re(args[0]->strVal);
                return ToValue::makeBool(std::regex_search(args[1]->strVal, re));
            } catch (std::regex_error& e) {
                throw ToRuntimeError("Invalid regex: " + std::string(e.what()));
            }
        }
    )});

    env->define("regex", mod);
}

// ========================
// Process Module
// ========================

void registerProcessModule(EnvPtr env) {
    auto mod = ToValue::makeDict({});

    // process.run(command) or process.run(command, args)
    // Returns {stdout, stderr, exit_code}
    mod->dictVal.push_back({"run", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.empty() || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("process.run() requires a string command");

            std::string cmd = args[0]->strVal;
            // If args list provided, append them
            if (args.size() >= 2 && args[1]->type == ToValue::Type::LIST) {
                for (auto& arg : args[1]->listVal) {
                    cmd += " " + arg->toString();
                }
            }

            // Redirect stderr to a temp file so we can capture both
            std::string stderrFile = "/tmp/to_proc_stderr_" + std::to_string(time(nullptr));
            std::string fullCmd = cmd + " 2>" + stderrFile;

            FILE* pipe = popen(fullCmd.c_str(), "r");
            if (!pipe) throw ToRuntimeError("Failed to run command: " + cmd);

            char buffer[4096];
            std::string stdoutStr;
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                stdoutStr += buffer;
            }
            int exitCode = pclose(pipe);
            exitCode = WEXITSTATUS(exitCode);

            // Read stderr
            std::string stderrStr;
            std::ifstream errFile(stderrFile);
            if (errFile.is_open()) {
                std::string line;
                while (std::getline(errFile, line)) {
                    stderrStr += line + "\n";
                }
                errFile.close();
            }
            std::filesystem::remove(stderrFile);

            // Trim trailing newline
            if (!stdoutStr.empty() && stdoutStr.back() == '\n') stdoutStr.pop_back();
            if (!stderrStr.empty() && stderrStr.back() == '\n') stderrStr.pop_back();

            std::vector<std::pair<std::string, ToValuePtr>> result;
            result.push_back({"stdout", ToValue::makeString(stdoutStr)});
            result.push_back({"stderr", ToValue::makeString(stderrStr)});
            result.push_back({"exit_code", ToValue::makeInt(exitCode)});
            return ToValue::makeDict(std::move(result));
        }
    )});

    // process.exec(command) — simple version, returns stdout only
    mod->dictVal.push_back({"exec", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.empty() || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("process.exec() requires a string command");
            std::string cmd = args[0]->strVal;
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) throw ToRuntimeError("Failed to run command: " + cmd);
            char buffer[4096];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }
            pclose(pipe);
            if (!result.empty() && result.back() == '\n') result.pop_back();
            return ToValue::makeString(result);
        }
    )});

    // process.env(name) — get environment variable
    mod->dictVal.push_back({"env", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.empty() || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("process.env() takes 1 string argument");
            const char* val = getenv(args[0]->strVal.c_str());
            if (!val) return ToValue::makeNone();
            return ToValue::makeString(val);
        }
    )});

    // process.exit(code)
    mod->dictVal.push_back({"exit", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            int code = 0;
            if (!args.empty() && args[0]->type == ToValue::Type::INT) code = (int)args[0]->intVal;
            exit(code);
            return ToValue::makeNone();
        }
    )});

    // process.args — command line arguments (empty for now, would need main to set them)
    mod->dictVal.push_back({"pid", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            return ToValue::makeInt(getpid());
        }
    )});

    env->define("process", mod);
}

// ========================
// Net Module (TCP/UDP sockets)
// ========================

#ifndef __EMSCRIPTEN__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// Helper: create a connection object (dict with fd-bound methods)
static ToValuePtr makeConnectionObject(int fd, const std::string& remoteAddr, int remotePort) {
    auto connFd = std::make_shared<int>(fd);
    auto conn = ToValue::makeDict({});

    conn->dictVal.push_back({"address", ToValue::makeString(remoteAddr)});
    conn->dictVal.push_back({"port", ToValue::makeInt(remotePort)});

    // conn.read(max_bytes?) — read data from connection
    conn->dictVal.push_back({"read", ToValue::makeBuiltin(
        [connFd](std::vector<ToValuePtr> args) -> ToValuePtr {
            int maxBytes = 4096;
            if (!args.empty() && args[0]->type == ToValue::Type::INT) maxBytes = (int)args[0]->intVal;
            std::vector<char> buf(maxBytes);
            ssize_t n = recv(*connFd, buf.data(), maxBytes, 0);
            if (n <= 0) return ToValue::makeString("");
            return ToValue::makeString(std::string(buf.data(), n));
        }
    )});

    // conn.read_line() — read until newline
    conn->dictVal.push_back({"read_line", ToValue::makeBuiltin(
        [connFd](std::vector<ToValuePtr> args) -> ToValuePtr {
            std::string line;
            char c;
            while (recv(*connFd, &c, 1, 0) == 1) {
                if (c == '\n') break;
                if (c != '\r') line += c;
            }
            return ToValue::makeString(line);
        }
    )});

    // conn.write(data) — send data
    conn->dictVal.push_back({"write", ToValue::makeBuiltin(
        [connFd](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.empty()) throw ToRuntimeError("conn.write() takes 1 argument");
            std::string data = args[0]->toString();
            ssize_t sent = send(*connFd, data.c_str(), data.size(), 0);
            return ToValue::makeInt(sent);
        }
    )});

    // conn.close()
    conn->dictVal.push_back({"close", ToValue::makeBuiltin(
        [connFd](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (*connFd >= 0) { close(*connFd); *connFd = -1; }
            return ToValue::makeNone();
        }
    )});

    return conn;
}

// Helper: create a TCP server object
static ToValuePtr makeTCPServerObject(int serverFd) {
    auto fd = std::make_shared<int>(serverFd);
    auto srv = ToValue::makeDict({});

    // server.accept() — wait for and accept a connection
    srv->dictVal.push_back({"accept", ToValue::makeBuiltin(
        [fd](std::vector<ToValuePtr> args) -> ToValuePtr {
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            int clientFd = accept(*fd, (struct sockaddr*)&clientAddr, &clientLen);
            if (clientFd < 0) throw ToRuntimeError("accept() failed");
            char addrStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, sizeof(addrStr));
            return makeConnectionObject(clientFd, addrStr, ntohs(clientAddr.sin_port));
        }
    )});

    // server.close()
    srv->dictVal.push_back({"close", ToValue::makeBuiltin(
        [fd](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (*fd >= 0) { close(*fd); *fd = -1; }
            return ToValue::makeNone();
        }
    )});

    return srv;
}

// Helper: create a UDP socket object
static ToValuePtr makeUDPSocketObject(int sockFd) {
    auto fd = std::make_shared<int>(sockFd);
    auto sock = ToValue::makeDict({});

    // sock.receive(max_bytes?) — receive data + sender info
    sock->dictVal.push_back({"receive", ToValue::makeBuiltin(
        [fd](std::vector<ToValuePtr> args) -> ToValuePtr {
            int maxBytes = 4096;
            if (!args.empty() && args[0]->type == ToValue::Type::INT) maxBytes = (int)args[0]->intVal;
            std::vector<char> buf(maxBytes);
            struct sockaddr_in senderAddr;
            socklen_t senderLen = sizeof(senderAddr);
            ssize_t n = recvfrom(*fd, buf.data(), maxBytes, 0,
                                  (struct sockaddr*)&senderAddr, &senderLen);
            if (n < 0) throw ToRuntimeError("recvfrom() failed");
            char addrStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &senderAddr.sin_addr, addrStr, sizeof(addrStr));
            return ToValue::makeDict({
                {"data", ToValue::makeString(std::string(buf.data(), n))},
                {"address", ToValue::makeString(addrStr)},
                {"port", ToValue::makeInt(ntohs(senderAddr.sin_port))}
            });
        }
    )});

    // sock.send(data, address, port)
    sock->dictVal.push_back({"send", ToValue::makeBuiltin(
        [fd](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() < 3) throw ToRuntimeError("sock.send() takes 3 arguments (data, address, port)");
            std::string data = args[0]->toString();
            std::string addr = args[1]->strVal;
            int port = (int)args[2]->intVal;

            struct sockaddr_in destAddr;
            memset(&destAddr, 0, sizeof(destAddr));
            destAddr.sin_family = AF_INET;
            destAddr.sin_port = htons(port);
            inet_pton(AF_INET, addr.c_str(), &destAddr.sin_addr);

            ssize_t sent = sendto(*fd, data.c_str(), data.size(), 0,
                                   (struct sockaddr*)&destAddr, sizeof(destAddr));
            return ToValue::makeInt(sent);
        }
    )});

    // sock.send_to(data, addr_dict) — send using address dict from receive
    sock->dictVal.push_back({"send_to", ToValue::makeBuiltin(
        [fd](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() < 2) throw ToRuntimeError("sock.send_to() takes 2 arguments (data, addr)");
            std::string data = args[0]->toString();
            std::string addr;
            int port = 0;
            if (args[1]->type == ToValue::Type::DICT) {
                for (auto& p : args[1]->dictVal) {
                    if (p.first == "address") addr = p.second->strVal;
                    if (p.first == "port") port = (int)p.second->intVal;
                }
            }
            struct sockaddr_in destAddr;
            memset(&destAddr, 0, sizeof(destAddr));
            destAddr.sin_family = AF_INET;
            destAddr.sin_port = htons(port);
            inet_pton(AF_INET, addr.c_str(), &destAddr.sin_addr);
            ssize_t sent = sendto(*fd, data.c_str(), data.size(), 0,
                                   (struct sockaddr*)&destAddr, sizeof(destAddr));
            return ToValue::makeInt(sent);
        }
    )});

    // sock.close()
    sock->dictVal.push_back({"close", ToValue::makeBuiltin(
        [fd](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (*fd >= 0) { close(*fd); *fd = -1; }
            return ToValue::makeNone();
        }
    )});

    return sock;
}

void registerNetModule(EnvPtr env) {
    auto mod = ToValue::makeDict({});

    // net.listen(protocol, port) — create a TCP or UDP server
    mod->dictVal.push_back({"listen", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() < 2) throw ToRuntimeError("net.listen() takes 2 arguments (protocol, port)");
            std::string proto = args[0]->strVal;
            int port = (int)args[1]->intVal;

            if (proto == "tcp") {
                int fd = socket(AF_INET, SOCK_STREAM, 0);
                if (fd < 0) throw ToRuntimeError("Failed to create TCP socket");
                int opt = 1;
                setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = INADDR_ANY;
                addr.sin_port = htons(port);

                if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                    close(fd);
                    throw ToRuntimeError("Failed to bind TCP socket to port " + std::to_string(port));
                }
                if (::listen(fd, 128) < 0) {
                    close(fd);
                    throw ToRuntimeError("Failed to listen on TCP socket");
                }
                return makeTCPServerObject(fd);

            } else if (proto == "udp") {
                int fd = socket(AF_INET, SOCK_DGRAM, 0);
                if (fd < 0) throw ToRuntimeError("Failed to create UDP socket");
                int opt = 1;
                setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = INADDR_ANY;
                addr.sin_port = htons(port);

                if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                    close(fd);
                    throw ToRuntimeError("Failed to bind UDP socket to port " + std::to_string(port));
                }
                return makeUDPSocketObject(fd);
            }

            throw ToRuntimeError("Unknown protocol: " + proto + " (use 'tcp' or 'udp')");
        }
    )});

    // net.connect(protocol, host, port) — connect to a remote server
    mod->dictVal.push_back({"connect", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.size() < 3) throw ToRuntimeError("net.connect() takes 3 arguments (protocol, host, port)");
            std::string proto = args[0]->strVal;
            std::string host = args[1]->strVal;
            int port = (int)args[2]->intVal;

            if (proto == "tcp") {
                int fd = socket(AF_INET, SOCK_STREAM, 0);
                if (fd < 0) throw ToRuntimeError("Failed to create socket");

                // Resolve hostname
                struct addrinfo hints, *result;
                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_STREAM;

                std::string portStr = std::to_string(port);
                if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0) {
                    close(fd);
                    throw ToRuntimeError("Cannot resolve host: " + host);
                }

                if (connect(fd, result->ai_addr, result->ai_addrlen) < 0) {
                    freeaddrinfo(result);
                    close(fd);
                    throw ToRuntimeError("Cannot connect to " + host + ":" + std::to_string(port));
                }
                freeaddrinfo(result);

                return makeConnectionObject(fd, host, port);

            } else if (proto == "udp") {
                int fd = socket(AF_INET, SOCK_DGRAM, 0);
                if (fd < 0) throw ToRuntimeError("Failed to create UDP socket");
                return makeUDPSocketObject(fd);
            }

            throw ToRuntimeError("Unknown protocol: " + proto);
        }
    )});

    // net.resolve(hostname) — DNS lookup
    mod->dictVal.push_back({"resolve", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr> args) -> ToValuePtr {
            if (args.empty() || args[0]->type != ToValue::Type::STRING)
                throw ToRuntimeError("net.resolve() takes 1 string argument");

            struct addrinfo hints, *result;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;

            if (getaddrinfo(args[0]->strVal.c_str(), nullptr, &hints, &result) != 0) {
                return ToValue::makeNone();
            }

            std::vector<ToValuePtr> addresses;
            for (auto* p = result; p != nullptr; p = p->ai_next) {
                char addrStr[INET_ADDRSTRLEN];
                struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
                inet_ntop(AF_INET, &ipv4->sin_addr, addrStr, sizeof(addrStr));
                addresses.push_back(ToValue::makeString(addrStr));
            }
            freeaddrinfo(result);

            if (addresses.empty()) return ToValue::makeNone();
            if (addresses.size() == 1) return addresses[0];
            return ToValue::makeList(std::move(addresses));
        }
    )});

    env->define("net", mod);
}
#else
// WASM stub
void registerNetModule(EnvPtr env) {
    auto mod = ToValue::makeDict({});
    mod->dictVal.push_back({"listen", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr>) -> ToValuePtr { throw ToRuntimeError("Networking not available in browser"); }
    )});
    mod->dictVal.push_back({"connect", ToValue::makeBuiltin(
        [](std::vector<ToValuePtr>) -> ToValuePtr { throw ToRuntimeError("Networking not available in browser"); }
    )});
    env->define("net", mod);
}
#endif
