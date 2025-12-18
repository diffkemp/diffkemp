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

void checkCall(const std::string &file, const std::vector<std::string> &args) {
    std::vector<char *> cArgs;
    cArgs.reserve(args.size() + 2);

    cArgs.push_back(const_cast<char *>(file.c_str()));

    for (const auto &arg : args) {
        cArgs.push_back(const_cast<char *>(arg.c_str()));
    }

    cArgs.push_back(nullptr);

    pid_t pid = fork();

    if (pid == -1) {
        throw std::runtime_error("Fork failed");
    }

    if (pid == 0) {
        execvp(file.c_str(), cArgs.data());
        perror(("execv failed for: " + file).c_str());
        _exit(127);
    }

    int status;
    pid_t waitResult = waitpid(pid, &status, 0);

    if (waitResult == -1) {
        throw CalledProcessError();
    }
    if (WIFEXITED(status)) {
        int exitStatus = WEXITSTATUS(status);
        if (exitStatus != 0) {
            throw CalledProcessError();
        }
    } else {
        throw CalledProcessError();
    }
}

int wrapper(int argc, char *argv[]) {
    WrapperArgs wrapperArgs = WrapperArgs(argc, argv);

    // Run GCC
    try {
        checkCall("gcc", wrapperArgs.clangArgs);
    } catch (const std::exception &e) {
        std::cerr << "cc_wrapper: warning: original build command failed"
                  << e.what() << std::endl;
        return 1;
    }

    // Analyze and modify parameters for clang (phase 1)
    std::vector<std::string> clangArgv;
    std::string clangBin = wrapperArgs.clang;
    std::string oldClang = clangBin;
    bool linkingWithSources = false;
    std::string outputFile;
    bool linking = !isInVector("-c", wrapperArgs.clangArgs);
    // Check if arguments contains C source files
    bool containsSource = false;

    for (std::size_t i = 0; i < wrapperArgs.clangArgs.size(); ++i) {
        std::string arg = wrapperArgs.clangArgs[i];
        if (wrapperArgs.clangDrop.find(arg) != wrapperArgs.clangDrop.end()) {
            continue;
        }

        bool isObjFile = (endsWith(arg, ".o") || endsWith(arg, ".lo")
                          || endsWith(arg, ".ko"));
        bool isSourceFile = endsWith(arg, ".c");
        containsSource = containsSource || isSourceFile;
        if (i > 0 && wrapperArgs.clangArgs[i - 1] == "-o") {
            if (isObjFile && !linking) {
                // Compiling to object file: swap .o with .ll
                arg = arg.substr(0, arg.rfind('.')) + ".ll";
            }
            if (!isObjFile && linking) {
                // Linking: add a .llw suffix (LLVM IR whole)
                arg = arg + ".llw";
            }
            outputFile = arg;
        } else if (isObjFile && linking) {
            // Input to linking phase: change suffix to .ll
            arg = arg.substr(0, arg.rfind('.')) + ".ll";
            clangBin = wrapperArgs.llvmLink;
        } else if (isSourceFile && linking) {
            // Mark as linking with sources to detect hybrid mode
            linkingWithSources = true;
        }
        clangArgv.push_back(arg);
    }

    if (linkingWithSources && clangBin == wrapperArgs.llvmLink) {
        // Compile/link mode with object files detected
        // Drop object files and revert to normal compiler/link mode
        clangBin = oldClang;
        std::vector<std::string> tmp;
        std::copy_if(clangArgv.begin(),
                     clangArgv.end(),
                     std::back_inserter(tmp),
                     [](const auto &arg) { return !endsWith(arg, ".ll"); });
        clangArgv = std::move(tmp);
    }

    // Do not continue if output is not .ll or .llw
    // Note: this means that this is netiher compilation nor linking
    if (outputFile.empty()
        || (!endsWith(outputFile, ".ll") && !endsWith(outputFile, ".llw"))) {
        return 0;
    }

    // Do not run clang on conftest files
    if (outputFile == "conftest.ll" || outputFile == "conftest.llw"
        || isInVector("conftest.c", wrapperArgs.clangArgs)) {
        return 0;
    }

    // Not compiling C source file
    if (!linking && !containsSource) {
        return 0;
    }

    // Record file in database
    std::vector<std::string> dbEntries;
    if (!outputFile.empty()) {
        std::string prefix = (clangBin != wrapperArgs.llvmLink) ? "o:" : "f:";
        dbEntries.push_back(prefix
                            + (fs::current_path() / outputFile).string());
    } else if (!linking) {
        // Compiling to default output file
        for (const std::string &arg : clangArgv) {
            if (!endsWith(arg, ".c")) {
                fs::path p(arg);
                p.replace_extension(".ll");
                dbEntries.push_back("o:" + (fs::current_path() / p).string());
            }
        }
    }

    // Analyze and modify parameters for clang (phase 2)
    if (clangBin != wrapperArgs.llvmLink) {
        // Note: clang uses the last specified optimization level so
        // extending with the default options must be done before
        // extending with the clang_append option.
        auto defaults = getClangDefaultOptions(!wrapperArgs.noOptOverride);
        clangArgv.insert(clangArgv.end(), defaults.begin(), defaults.end());
        clangArgv.insert(clangArgv.end(),
                         wrapperArgs.clangAppend.begin(),
                         wrapperArgs.clangAppend.end());
        // TODO: allow compiling into binary IR
    } else {
        // Keep only arguments with input files (and llvm-link itself)
        std::vector<std::string> tempArgs;
        for (const auto &arg : clangArgv) {
            if (endsWith(arg, ".ll") || endsWith(arg, ".llw") || arg == "-o") {
                tempArgs.push_back(arg);
            }
        }

        // Remove non-existing files
        // Note: these might have been e.g. generated from assembly
        std::vector<std::string> newArgs = {"-S"};
        bool outputFlag = false;
        for (const auto &arg : tempArgs) {
            if (outputFlag || fs::exists(arg) || arg == "-o") {
                newArgs.push_back(arg);
            }
            outputFlag = arg == "-o";
        }
        clangArgv = std::move(newArgs);
    }

    if (wrapperArgs.debug) {
        std::cout << "Wrapper calling:";
        for (const auto &arg : clangArgv) {
            std::cout << arg;
        }
        std::cout << "\n";
    }

    // Run clang
    try {
        checkCall(clangBin, clangArgv);
    } catch (const std::exception &e) {
        std::cerr << "cc_wrapper: warning: clang failed" << e.what()
                  << std::endl;
        return 0;
    }

    // Update the database file
    std::ofstream db(wrapperArgs.dbFilename, std::ios::app);
    if (db) {
        for (const auto &entry : dbEntries) {
            std::string path = entry.substr(2);
            if (fs::exists(path)) {
                db << entry << "\n";
            }
        }
    } else {
        std::cerr << "cc_wrapper: warning: cannot open DB file for append:"
                  << wrapperArgs.dbFilename << std::endl;
    }

    return 0;
}

int main(int argc, char *argv[]) { return wrapper(argc, argv); }
