#include "cc_wrapper_utils.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

class CalledProcessError : public std::exception {};

void check_call(const std::string &file, const std::vector<std::string> &args) {
    std::vector<char *> c_args;
    c_args.reserve(args.size() + 2);

    c_args.push_back(const_cast<char *>(file.c_str()));

    for (const std::string &arg : args) {
        c_args.push_back(const_cast<char *>(arg.c_str()));
    }

    c_args.push_back(NULL);

    pid_t pid = fork();

    if (pid == -1) {
        throw std::runtime_error("Fork failed");
    }

    if (pid == 0) {
        execvp(file.c_str(), c_args.data());
        perror(("execv failed for: " + file).c_str());
        _exit(127);
    }

    int status;
    pid_t wait_result = waitpid(pid, &status, 0);

    if (wait_result == -1) {
        throw CalledProcessError();
    }
    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        if (exit_status != 0) {
            throw CalledProcessError();
        }
    } else {
        throw CalledProcessError();
    }
}

int wrapper(int argc, char *argv[]) {
    auto [wrapper_args, argv_vec] = parse_args(argc, argv);
    Own_args own_args(wrapper_args);

    std::vector<std::string> append = split(own_args.clang_append, ',');
    std::vector<std::string> drop = split(own_args.clang_drop, ',');

    // Run GCC
    std::vector<std::string> gcc_args;
    gcc_args.reserve(argv_vec.size() + 1);
    for (std::string arg : argv_vec) {
        gcc_args.push_back(arg);
    }

    try {
        check_call("gcc", gcc_args);
    } catch (const std::exception &e) {
        std::cerr << "cc_wrapper: warning: original build command failed"
                  << e.what() << std::endl;
        return 1;
    }

    // Analyze and modify parameters for clang (phase 1)
    std::vector<std::string> clang_argv;
    std::string clang_bin = own_args.clang;
    std::string old_clang = clang_bin;
    bool linking_with_sources = false;
    std::string output_file;
    bool linking = !is_in_vector("-c", argv_vec);
    // Check if arguments contains C source files
    bool contains_source = false;

    for (int i = 0; i < argv_vec.size(); ++i) {
        std::string arg = argv_vec[i];
        if (is_in_vector(arg, drop))
            continue;

        bool is_obj_file = (ends_with(arg, ".o") || ends_with(arg, ".lo")
                            || ends_with(arg, ".ko"));
        bool is_source_file = ends_with(arg, ".c");
        contains_source = contains_source || is_source_file;
        if (i > 1 && argv_vec[i - 1] == "-o") {
            if (is_obj_file && !linking)
                // Compiling to object file: swap .o with .ll
                arg = arg.substr(0, arg.rfind('.')) + ".ll";
            if (!is_obj_file && linking)
                // Linking: add a .llw suffix (LLVM IR whole)
                arg = arg + ".llw";
            output_file = arg;
        } else if (is_obj_file && linking) {
            // Input to linkin phase: change suffix to .ll
            arg = arg.substr(0, arg.rfind('.')) + ".ll";
            clang_bin = own_args.llvm_link;
        } else if (is_source_file && linking)
            // Mark as linking with sources to detect hybrid mode
            linking_with_sources = true;
        clang_argv.push_back(arg);
    }

    if (linking_with_sources && clang_bin == own_args.llvm_link) {
        // Compile/link mode with object files deteceted
        // Drop object files and revert to normal compiler/link mode
        clang_bin = old_clang;
        std::vector<std::string> tmp;
        for (const std::string &arg : clang_argv) {
            if (!ends_with(arg, ".ll"))
                tmp.push_back(arg);
        }
        clang_argv = std::move(tmp);
    }

    // Do not continue if output is not .ll or .llw
    // Note: this means that this is netiher compilation nor linking
    if (!output_file.empty() && !ends_with(output_file, ".ll")
        && !ends_with(output_file, ".llw"))
        return 0;

    // Do not run clang on conftest files
    if (output_file == "conftest.ll" || output_file == "conftest.llw"
        || is_in_vector("conftest.c", argv_vec))
        return 0;

    // Not compiling C source file
    if (!linking && !contains_source)
        return 0;

    // Record file in database
    std::vector<std::string> db_entries;
    if (!output_file.empty()) {
        std::string prefix = clang_bin != own_args.llvm_link ? "o:" : "f:";
        db_entries.push_back(prefix + fs::current_path().string() + "/"
                             + output_file);
    } else if (!linking) {
        // Compiling to default output file
        for (const std::string &arg : clang_argv) {
            if (!ends_with(arg, ".c")) {
                std::string base = arg;
                auto pos = base.rfind('.');
                if (pos != std::string::npos)
                    base = base.substr(0, pos);
                db_entries.push_back("o:" + fs::current_path().string() + "/"
                                     + base + ".ll");
            }
        }
    }

    // Analyze and modify parameters for clang (phase 2)
    if (clang_bin != own_args.llvm_link) {
        // Note: clang uses the last specified optimization level so
        // extending with the default options must be done before
        // extending with the clang_append option.
        auto defaults = get_clang_default_options(!own_args.no_opt_override);
        clang_argv.insert(clang_argv.end(), defaults.begin(), defaults.end());
        clang_argv.insert(clang_argv.end(), append.begin(), append.end());
        // TODO: allow compiling into binary IR
    } else {
        // Keep only arguments with input files (and llvm-link itself)
        std::vector<std::string> temp_args;
        for (const auto &arg : clang_argv) {
            if (ends_with(arg, ".ll") || ends_with(arg, ".llw")
                || arg == "-o") {
                temp_args.push_back(arg);
            }
        }

        // Remove non-existent files
        // Note: these might have been e.g. generated from assembly
        std::vector<std::string> new_args = {"-S"};
        bool output_flag = false;
        for (size_t i = 0; i < temp_args.size(); ++i) {
            const auto &a = temp_args[i];
            if (output_flag || fs::exists(a) || a == "-o") {
                new_args.push_back(a);
                if (output_flag)
                    output_flag = false;
            }
            if (a == "-o")
                output_flag = true;
        }
        clang_argv = std::move(new_args);
    }

    if (own_args.debug) {
        printf("Wrapper calling:");
        for (const auto &arg : clang_argv) {
            printf(" %s", arg.c_str());
        }
        printf("\n");
    }

    // Run clang
    try {
        check_call(clang_bin, clang_argv);
    } catch (const std::exception &e) {
        std::cerr << "cc_wrapper: warning: clang failed" << e.what()
                  << std::endl;
        return 0;
    }

    // Update databse file
    std::ofstream db(own_args.db_filename, std::ios::app);
    if (db) {
        for (const auto &entry : db_entries) {
            std::string path = entry.substr(2);
            if (fs::exists(path))
                db << entry << "\n";
        }
    } else {
        std::cerr << "cc_wrapper: warning: cannot open DB file for append:"
                  << own_args.db_filename << std::endl;
    }

    return 0;
}

int main(int argc, char *argv[]) { return wrapper(argc, argv); }
