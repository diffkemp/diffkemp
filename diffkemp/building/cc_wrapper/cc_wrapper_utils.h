#ifndef CC_WRAPPER_UTILS_H
#define CC_WRAPPER_UTILS_H

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Utility functions for the cc_wrapper

std::vector<std::string> split(const std::string &s, char delim);
bool ends_with(const std::string &str, const std::string &suffix);
std::vector<std::string> get_clang_default_options(bool default_optim = false);
bool is_executable(const std::string &path);
std::string find_in_path(const std::string &name);
bool is_in_vector(const std::string &s, const std::vector<std::string> &vector);
struct Own_args {
    std::string db_filename;
    std::string clang;
    std::string clang_append;
    std::string clang_drop;
    bool debug;
    std::string llvm_link;
    std::string llvm_dis;
    bool no_opt_override;

    Own_args(std::unordered_map<std::string, std::string> &own_args) {
        db_filename = own_args["dbf"];
        clang = own_args["clang"];
        clang_append = own_args["cla"];
        clang_drop = own_args["cld"];
        debug = (own_args["debug"] == "1");
        llvm_link = own_args["llink"];
        llvm_dis = own_args["lldis"];
        no_opt_override = (own_args["noo"] == "1");
    }
};
std::pair<std::unordered_map<std::string, std::string>,
          std::vector<std::string>>
        parse_args(int argc, char *argv[]);

#endif // CC_WRAPPER_UTILS_H
