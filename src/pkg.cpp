#include "pkg.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <regex>

namespace fs = std::filesystem;

// ========================
// Helpers
// ========================

// Parse a package spec like "user/repo" or "user/repo@v1.0.0"
// Returns {user, repo, version}
static void parseSpec(const std::string& spec, std::string& user, std::string& repo, std::string& version) {
    std::string s = spec;
    version = "";

    // Split on @ for version
    size_t at = s.find('@');
    if (at != std::string::npos) {
        version = s.substr(at + 1);
        s = s.substr(0, at);
    }

    // Split on / for user/repo
    size_t slash = s.find('/');
    if (slash == std::string::npos) {
        user = "";
        repo = s;
    } else {
        user = s.substr(0, slash);
        repo = s.substr(slash + 1);
    }
}

// Read to.pkg manifest
struct Manifest {
    std::string name;
    std::string version = "0.1.0";
    std::vector<std::string> dependencies;
};

static Manifest readManifest(const std::string& path = "to.pkg") {
    Manifest m;
    if (!fs::exists(path)) return m;

    std::ifstream file(path);
    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    // Simple line-based parser: name = "foo", version = "1.0", dependencies = [...]
    std::regex nameRe("name\\s*=\\s*\"([^\"]+)\"");
    std::regex versionRe("version\\s*=\\s*\"([^\"]+)\"");
    std::regex depsRe("dependencies\\s*=\\s*\\[([^\\]]*)\\]");

    std::smatch match;
    if (std::regex_search(content, match, nameRe)) m.name = match[1];
    if (std::regex_search(content, match, versionRe)) m.version = match[1];
    if (std::regex_search(content, match, depsRe)) {
        std::string depList = match[1];
        std::regex itemRe("\"([^\"]+)\"");
        auto itBegin = std::sregex_iterator(depList.begin(), depList.end(), itemRe);
        auto itEnd = std::sregex_iterator();
        for (auto it = itBegin; it != itEnd; ++it) {
            m.dependencies.push_back((*it)[1]);
        }
    }
    return m;
}

static void writeManifest(const Manifest& m, const std::string& path = "to.pkg") {
    std::ofstream file(path);
    file << "~ To package manifest\n\n";
    file << "name = \"" << m.name << "\"\n";
    file << "version = \"" << m.version << "\"\n";
    file << "dependencies = [";
    for (size_t i = 0; i < m.dependencies.size(); i++) {
        if (i > 0) file << ", ";
        file << "\n  \"" << m.dependencies[i] << "\"";
    }
    if (!m.dependencies.empty()) file << "\n";
    file << "]\n";
}

// Download a package from GitHub
static bool downloadPackage(const std::string& user, const std::string& repo,
                             const std::string& version, const std::string& destDir) {
    // Use git clone. If version is specified, clone then checkout that tag.
    std::string url = "https://github.com/" + user + "/" + repo + ".git";
    std::string cmd;

    if (version.empty()) {
        // Clone latest, shallow
        cmd = "git clone --depth 1 --quiet \"" + url + "\" \"" + destDir + "\" 2>&1";
    } else {
        // Clone a specific tag/branch
        cmd = "git clone --depth 1 --branch \"" + version + "\" --quiet \"" + url + "\" \"" + destDir + "\" 2>&1";
    }

    int result = system(cmd.c_str());
    if (result != 0) {
        // If branch-specific clone fails, try full clone then checkout
        if (!version.empty()) {
            fs::remove_all(destDir);
            cmd = "git clone --quiet \"" + url + "\" \"" + destDir + "\" 2>&1 && cd \"" + destDir + "\" && git checkout --quiet \"" + version + "\" 2>&1";
            result = system(cmd.c_str());
        }
    }

    if (result == 0) {
        // Remove .git to save space
        fs::remove_all(fs::path(destDir) / ".git");
        return true;
    }
    return false;
}

// ========================
// Commands
// ========================

int pkgGet(const std::string& spec) {
    std::string user, repo, version;
    parseSpec(spec, user, repo, version);

    if (user.empty() || repo.empty()) {
        std::cerr << "Error: package spec must be 'user/repo' or 'user/repo@version'\n";
        std::cerr << "Got: '" << spec << "'\n";
        return 1;
    }

    // Create to_modules directory
    fs::create_directories("to_modules");

    std::string destDir = "to_modules/" + repo;

    // Check if already installed
    if (fs::exists(destDir)) {
        std::cout << "Package '" << repo << "' is already installed. Updating...\n";
        fs::remove_all(destDir);
    }

    std::cout << "Fetching " << user << "/" << repo;
    if (!version.empty()) std::cout << "@" << version;
    std::cout << "...\n";

    if (!downloadPackage(user, repo, version, destDir)) {
        std::cerr << "Error: failed to download " << spec << "\n";
        std::cerr << "Check that github.com/" << user << "/" << repo << " exists\n";
        return 1;
    }

    std::cout << "  Installed to " << destDir << "\n";

    // Check for nested dependencies
    std::string nestedManifest = destDir + "/to.pkg";
    if (fs::exists(nestedManifest)) {
        Manifest nested = readManifest(nestedManifest);
        if (!nested.dependencies.empty()) {
            std::cout << "  Installing " << nested.dependencies.size() << " nested dependencies...\n";
            for (auto& dep : nested.dependencies) {
                pkgGet(dep);
            }
        }
    }

    // Update local to.pkg if it exists
    if (fs::exists("to.pkg")) {
        Manifest m = readManifest();
        // Only add if not already present
        bool found = false;
        for (auto& d : m.dependencies) {
            if (d == spec || d.find(user + "/" + repo) != std::string::npos) {
                found = true;
                break;
            }
        }
        if (!found) {
            m.dependencies.push_back(spec);
            writeManifest(m);
            std::cout << "  Added to to.pkg\n";
        }
    }

    std::cout << "\nUse it in your code:\n";
    std::cout << "  use <name> from \"" << repo << "\"\n";

    return 0;
}

int pkgRemove(const std::string& name) {
    std::string destDir = "to_modules/" + name;

    if (!fs::exists(destDir)) {
        std::cerr << "Error: package '" << name << "' is not installed\n";
        return 1;
    }

    fs::remove_all(destDir);
    std::cout << "Removed " << name << "\n";

    // Update to.pkg
    if (fs::exists("to.pkg")) {
        Manifest m = readManifest();
        std::vector<std::string> filtered;
        for (auto& d : m.dependencies) {
            // Keep entries that don't match this repo name
            std::string user, repo, version;
            parseSpec(d, user, repo, version);
            if (repo != name) filtered.push_back(d);
        }
        m.dependencies = filtered;
        writeManifest(m);
    }

    return 0;
}

int pkgList() {
    if (!fs::exists("to_modules")) {
        std::cout << "No packages installed. Use 'to get user/repo' to install one.\n";
        return 0;
    }

    std::vector<std::string> packages;
    for (auto& entry : fs::directory_iterator("to_modules")) {
        if (entry.is_directory()) {
            packages.push_back(entry.path().filename().string());
        }
    }

    if (packages.empty()) {
        std::cout << "No packages installed.\n";
        return 0;
    }

    std::cout << "Installed packages (" << packages.size() << "):\n";
    for (auto& pkg : packages) {
        std::cout << "  " << pkg;

        // Try to read the package's own manifest for version info
        std::string pkgManifest = "to_modules/" + pkg + "/to.pkg";
        if (fs::exists(pkgManifest)) {
            Manifest m = readManifest(pkgManifest);
            if (!m.version.empty()) std::cout << " (v" << m.version << ")";
        }
        std::cout << "\n";
    }

    return 0;
}

int pkgInit() {
    if (fs::exists("to.pkg")) {
        std::cerr << "Error: to.pkg already exists\n";
        return 1;
    }

    Manifest m;
    m.name = fs::current_path().filename().string();
    m.version = "0.1.0";

    writeManifest(m);
    std::cout << "Created to.pkg\n";
    std::cout << "  name: " << m.name << "\n";
    std::cout << "  version: " << m.version << "\n";

    return 0;
}
