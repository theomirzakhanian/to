#include <emscripten.h>
#include <string>
#include <sstream>
#include <iostream>
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/interpreter.h"

// Capture stdout to a string buffer
static std::string outputBuffer;
static std::streambuf* originalCout = nullptr;
static std::ostringstream captureStream;

static void beginCapture() {
    outputBuffer.clear();
    captureStream.str("");
    captureStream.clear();
    originalCout = std::cout.rdbuf(captureStream.rdbuf());
}

static std::string endCapture() {
    std::cout.rdbuf(originalCout);
    return captureStream.str();
}

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* to_eval(const char* source) {
    static std::string result;
    beginCapture();

    try {
        std::string src(source);
        src += "\n";
        Lexer lexer(src, "<playground>");
        auto tokens = lexer.tokenize();
        Parser parser(tokens, "<playground>");
        auto ast = parser.parse();
        Interpreter interp("<playground>");
        interp.execute(ast);
        result = endCapture();
    } catch (ToError& e) {
        result = endCapture();
        result += e.format() + "\n";
    } catch (ToRuntimeError& e) {
        result = endCapture();
        result += e.format() + "\n";
    } catch (std::runtime_error& e) {
        result = endCapture();
        result += std::string("Error: ") + e.what() + "\n";
    } catch (...) {
        result = endCapture();
        result += "Unknown error\n";
    }

    return result.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* to_version() {
    return "0.2.0";
}

} // extern "C"
