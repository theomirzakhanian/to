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
