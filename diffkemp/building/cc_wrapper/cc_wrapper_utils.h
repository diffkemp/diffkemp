#ifndef CC_WRAPPER_UTILS_H
#define CC_WRAPPER_UTILS_H

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Utility functions for the cc_wrapper
template <typename Container> Container split(const std::string &s, char delim);
bool endsWith(const std::string &str, const std::string &suffix) noexcept;
std::vector<std::string> getClangDefaultOptions(bool defaultOptim = false);
bool isExecutable(const std::string &path) noexcept;
bool isInVector(const std::string &s,
                const std::vector<std::string> &vector) noexcept;
struct WrapperArgs {
    std::string dbFilename;
    std::string clang;
    std::vector<std::string> clangAppend;
    std::unordered_set<std::string> clangDrop;
    bool debug = false;
    std::string llvmLink;
    std::string llvmDis;
    bool noOptOverride = false;
    std::vector<std::string> clangArgs;

    WrapperArgs(int argc, char *argv[]);
};

#endif // CC_WRAPPER_UTILS_H
