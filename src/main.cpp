#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include "codegen.h"
#include "error.h"
#include "pkg.h"
#include "formatter.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

const std::string VERSION = "0.2.0";

void printUsage() {
    std::cout << "To Programming Language v" << VERSION << "\n\n"
              << "Usage:\n"
              << "  to                            Start interactive REPL\n"
              << "  to run <file.to>              Run a .to file\n"
              << "  to build <file.to>            Compile to native binary\n"
              << "  to build <file.to> -o <name>  Compile with custom output name\n"
              << "  to check <file.to>            Check for errors without running\n"
              << "  to debug <file.to>            Run with debugger\n"
              << "  to test <file.to>             Run test functions\n"
              << "  to init                       Create to.pkg in current directory\n"
              << "  to get <user/repo>            Install a package from GitHub\n"
              << "  to get <user/repo>@<version>  Install a specific version\n"
              << "  to remove <name>              Uninstall a package\n"
              << "  to list                       List installed packages\n"
              << "  to fmt <file.to>              Format a .to file\n"
              << "  to version                    Print version\n";
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << path << "'\n";
        exit(1);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int cmdRun(const std::string& filepath, bool debug = false) {
    std::string source = readFile(filepath);
    try {
        Lexer lexer(source, filepath);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, filepath);
        auto ast = parser.parse();
        Interpreter interp(filepath);
        if (debug) {
            interp.debugMode = true;
            interp.stepMode = true;
            std::cout << "To Debugger — " << filepath << "\n";
            std::cout << "Type 'help' for commands. Press enter to step.\n\n";
        }
        interp.execute(ast);
        return 0;
    } catch (ToRuntimeError& e) {
        std::cerr << e.format() << "\n";
        return 1;
    } catch (ToError& e) {
        std::cerr << e.format() << "\n";
        return 1;
    } catch (std::runtime_error& e) {
        std::cerr << "Runtime error: " << e.what() << "\n";
        return 1;
    }
}

int cmdDebug(const std::string& filepath) {
    return cmdRun(filepath, true);
}

int cmdBuild(const std::string& filepath, const std::string& outputName) {
    std::string source = readFile(filepath);
    try {
        Lexer lexer(source, filepath);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, filepath);
        auto ast = parser.parse();

        CodeGenerator codegen(filepath);
        std::string cSource = codegen.generate(ast);

        std::string output = outputName;
        if (output.empty()) {
            output = filepath;
            if (output.size() > 3 && output.substr(output.size() - 3) == ".to") {
                output = output.substr(0, output.size() - 3);
            }
        }

        std::cout << "Compiling " << filepath << " -> " << output << "\n";
        if (codegen.compile(cSource, output)) {
            std::cout << "Build successful: " << output << "\n";
            return 0;
        } else {
            std::cerr << "Build failed\n";
            return 1;
        }
    } catch (ToError& e) {
        std::cerr << e.format() << "\n";
        return 1;
    } catch (std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

int cmdCheck(const std::string& filepath) {
    std::string source = readFile(filepath);
    try {
        Lexer lexer(source, filepath);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, filepath);
        auto ast = parser.parse();
        std::cout << "No errors found in " << filepath << "\n";
        return 0;
    } catch (ToError& e) {
        std::cerr << e.format() << "\n";
        return 1;
    }
}

int cmdTest(const std::string& filepath) {
    std::string source = readFile(filepath);
    try {
        Lexer lexer(source, filepath);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, filepath);
        auto ast = parser.parse();
        Interpreter interp(filepath);
        interp.execute(ast);

        // Find all functions starting with "test_" and run them
        auto env = interp.getGlobalEnv();
        int passed = 0, failed = 0;
        std::vector<std::string> failures;

        // Collect test functions from the AST
        std::vector<std::string> testNames;
        for (auto& stmt : ast->statements) {
            if (stmt->type == NodeType::FunctionDef && stmt->name.substr(0, 5) == "test_") {
                testNames.push_back(stmt->name);
            }
        }

        if (testNames.empty()) {
            std::cout << "No test functions found (functions must start with 'test_')\n";
            return 0;
        }

        std::cout << "Running " << testNames.size() << " tests in " << filepath << "...\n\n";

        for (auto& name : testNames) {
            auto funcVal = env->get(name);
            if (!funcVal) continue;

            try {
                interp.callFunction(funcVal, {}, 0);
                std::cout << "  \u2713 " << name << "\n";
                passed++;
            } catch (ToRuntimeError& e) {
                std::cout << "  \u2717 " << name << " — " << e.detail << "\n";
                failed++;
                failures.push_back(name);
            } catch (std::runtime_error& e) {
                std::cout << "  \u2717 " << name << " — " << e.what() << "\n";
                failed++;
                failures.push_back(name);
            }
        }

        std::cout << "\n  " << passed << " passed, " << failed << " failed\n";

        if (!failures.empty()) {
            std::cout << "\n  Failed:\n";
            for (auto& f : failures) {
                std::cout << "    - " << f << "\n";
            }
        }

        return failed > 0 ? 1 : 0;
    } catch (ToError& e) {
        std::cerr << e.format() << "\n";
        return 1;
    } catch (std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

int cmdRepl() {
    std::cout << "To v" << VERSION << " — Interactive REPL\n";
    std::cout << "Type 'exit' or press Ctrl+C to quit.\n\n";

    Interpreter interp("<repl>");
    auto env = interp.getGlobalEnv();

    while (true) {
        std::cout << "to> ";
        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }

        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        if (line == "exit" || line == "quit") break;

        // Check if we need more lines (line ends with ':')
        std::string fullSource = line;
        if (!line.empty() && line.back() == ':') {
            // Multi-line block input
            while (true) {
                std::cout << "... ";
                std::string next;
                if (!std::getline(std::cin, next)) break;
                if (next.empty() || (next.find_first_not_of(" \t") == std::string::npos)) {
                    break; // empty line ends the block
                }
                fullSource += "\n" + next;
            }
        }

        fullSource += "\n";

        try {
            Lexer lexer(fullSource, "<repl>");
            auto tokens = lexer.tokenize();
            Parser parser(tokens, "<repl>");
            auto ast = parser.parse();

            // For single expressions, print the result
            if (ast->statements.size() == 1) {
                auto& stmt = ast->statements[0];
                if (stmt->type == NodeType::ExpressionStmt) {
                    auto val = interp.eval(stmt->value, env);
                    if (val->type != ToValue::Type::NONE) {
                        std::cout << val->toString() << "\n";
                    }
                    continue;
                }
            }

            interp.execute(ast);
        } catch (ToError& e) {
            std::cerr << "Error: " << e.detail << "\n";
        } catch (std::runtime_error& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        return cmdRepl();
    }

    std::string command = argv[1];

    if (command == "version" || command == "--version" || command == "-v") {
        std::cout << "To v" << VERSION << "\n";
        return 0;
    }

    if (command == "help" || command == "--help" || command == "-h") {
        printUsage();
        return 0;
    }

    if (command == "run") {
        if (argc < 3) {
            std::cerr << "Error: Missing file argument\n";
            std::cerr << "Usage: to run <file.to>\n";
            return 1;
        }
        return cmdRun(argv[2]);
    }

    if (command == "build") {
        if (argc < 3) {
            std::cerr << "Error: Missing file argument\n";
            std::cerr << "Usage: to build <file.to> [-o output]\n";
            return 1;
        }
        std::string output;
        if (argc >= 5 && std::string(argv[3]) == "-o") {
            output = argv[4];
        }
        return cmdBuild(argv[2], output);
    }

    if (command == "check") {
        if (argc < 3) {
            std::cerr << "Error: Missing file argument\n";
            std::cerr << "Usage: to check <file.to>\n";
            return 1;
        }
        return cmdCheck(argv[2]);
    }

    if (command == "debug") {
        if (argc < 3) {
            std::cerr << "Error: Missing file argument\n";
            std::cerr << "Usage: to debug <file.to>\n";
            return 1;
        }
        return cmdDebug(argv[2]);
    }

    if (command == "test") {
        if (argc < 3) {
            std::cerr << "Error: Missing file argument\n";
            std::cerr << "Usage: to test <file.to>\n";
            return 1;
        }
        return cmdTest(argv[2]);
    }

    if (command == "get") {
        if (argc < 3) {
            std::cerr << "Error: Missing package spec\n";
            std::cerr << "Usage: to get <user/repo>[@version]\n";
            return 1;
        }
        return pkgGet(argv[2]);
    }

    if (command == "remove" || command == "uninstall") {
        if (argc < 3) {
            std::cerr << "Error: Missing package name\n";
            std::cerr << "Usage: to remove <name>\n";
            return 1;
        }
        return pkgRemove(argv[2]);
    }

    if (command == "list") {
        return pkgList();
    }

    if (command == "init") {
        return pkgInit();
    }

    if (command == "fmt" || command == "format") {
        if (argc < 3) {
            std::cerr << "Error: Missing file argument\n";
            std::cerr << "Usage: to fmt <file.to>\n";
            return 1;
        }
        std::string filepath = argv[2];
        std::string source = readFile(filepath);
        try {
            Lexer lexer(source, filepath);
            auto tokens = lexer.tokenize();
            Parser parser(tokens, filepath);
            auto ast = parser.parse();
            Formatter formatter;
            std::string formatted = formatter.format(ast);
            // Write back
            std::ofstream out(filepath);
            out << formatted;
            std::cout << "Formatted " << filepath << "\n";
            return 0;
        } catch (ToError& e) {
            std::cerr << e.format() << "\n";
            return 1;
        }
    }

    std::cerr << "Unknown command: " << command << "\n";
    printUsage();
    return 1;
}
