#include <filesystem>
#include <sstream>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
        out.push_back(item);
    return out;
}

bool ends_with(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size()
           && str.compare(str.size() - suffix.size(), suffix.size(), suffix)
                      == 0;
}

// When making changes to this function, same changes need to be made
// in diffkemp/llvm_ir/compiler.py
std::vector<std::string> get_clang_default_options(bool default_optim = false) {
    std::vector<std::string> options;
    options = {
            "-S", "-emit-llvm", "-g", "-fdebug-macro", "-Wno-format-security"};
    if (default_optim) {
        options.push_back("-O1");
        options.push_back("-Xclang");
        options.push_back("-disable-llvm-passes");
    }
    return options;
}

bool is_executable(const std::string &path) {
    return access(path.c_str(), X_OK) == 0;
}

std::string find_in_path(const std::string &name) {
    const char *env_path = std::getenv("PATH");
    std::string pathstr =
            env_path ? env_path
                     : std::string(getenv("PATH") ? getenv("PATH") : "");
    if (pathstr.empty())
        pathstr = std::string(::getenv("PATH") ? ::getenv("PATH") : "");
    std::stringstream ss(pathstr);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty())
            continue;
        std::string full = dir + "/" + name;
        if (std::filesystem::exists(full) && is_executable(full))
            return full;
    }
    return "";
}

bool is_in_vector(const std::string &s,
                  const std::vector<std::string> &vector) {
    for (const std::string &curr_s : vector) {
        if (curr_s == s)
            return true;
    }
    return false;
}

std::pair<std::unordered_map<std::string, std::string>,
          std::vector<std::string>>
        parse_args(int argc, char *argv[]) {
    std::unordered_map<std::string, std::string> own_args;
    std::vector<std::string> clang_args;

    bool args_switch = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") {
            args_switch = true;
            continue;
        }
        if (args_switch) {
            clang_args.push_back(arg);
        } else {
            auto pos = arg.find('=');
            if (pos != std::string::npos) {
                size_t key_start = arg.find_first_not_of('-');

                std::string key = arg.substr(key_start, pos - key_start);
                std::string value = arg.substr(pos + 1);
                own_args[key] = value;
            }
        }
    }
    return {own_args, clang_args};
}
