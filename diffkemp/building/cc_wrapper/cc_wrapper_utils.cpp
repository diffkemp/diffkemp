#include "cc_wrapper_utils.h"
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

template <typename Conatiner>
Conatiner split(const std::string &s, char delim) {
    Conatiner out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        out.insert(out.end(), item);
    }
    return out;
}

bool endsWith(const std::string &str, const std::string &suffix) noexcept {
    return str.size() >= suffix.size()
           && str.compare(str.size() - suffix.size(), suffix.size(), suffix)
                      == 0;
}

// When making changes to this function, same changes need to be made
// in diffkemp/llvm_ir/compiler.py
std::vector<std::string> getClangDefaultOptions(bool defaultOptim) {
    std::vector<std::string> options = {
            "-S", "-emit-llvm", "-g", "-fdebug-macro", "-Wno-format-security"};
    if (defaultOptim) {
        options.push_back("-O1");
        options.push_back("-Xclang");
        options.push_back("-disable-llvm-passes");
    }
    return options;
}

bool isExecutable(const std::string &path) noexcept {
    return access(path.c_str(), X_OK) == 0;
}

bool isInVector(const std::string &s,
                const std::vector<std::string> &vector) noexcept {
    return std::find(vector.begin(), vector.end(), s) != vector.end();
}

WrapperArgs::WrapperArgs(int argc, char *argv[]) {
    bool argsSwitch = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") {
            argsSwitch = true;
            continue;
        }
        if (argsSwitch) {
            this->clangArgs.push_back(arg);
        } else {
            auto pos = arg.find('=');
            if (pos != std::string::npos) {
                size_t keyStart = arg.find_first_not_of('-');
                std::string key = arg.substr(keyStart, pos - keyStart);
                if (key == "cla") {
                    this->clangAppend = split<std::vector<std::string>>(
                            arg.substr(pos + 1), ',');
                } else if (key == "cld") {
                    this->clangDrop = split<std::unordered_set<std::string>>(
                            arg.substr(pos + 1), ',');
                } else {
                    std::string value = arg.substr(pos + 1);
                    if (key == "dbf")
                        this->dbFilename = value;
                    else if (key == "clang")
                        this->clang = value;
                    else if (key == "debug")
                        this->debug = value == "1";
                    else if (key == "llink")
                        this->llvmLink = value;
                    else if (key == "lldis")
                        this->llvmDis = value;
                    else if (key == "noo")
                        this->noOptOverride = value == "1";
                }
            }
        }
    }
}
