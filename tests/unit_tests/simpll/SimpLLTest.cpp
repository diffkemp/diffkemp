#include "Logger.h"

#include <cerrno>
#include <gtest/gtest.h>
#include <ostream>

/// Sets logging verbosity based on `SIMPLL_VERBOSITY` env. var.
/// Returns true if successful, otherwise returns false.
bool trySetVerbosity() {
    const char *verbosity = std::getenv("SIMPLL_VERBOSITY");
    if (!verbosity)
        return true;

    int level = 0;
    try {
        level = std::stoi(verbosity);
    } catch (const std::invalid_argument &e) {
        std::cerr << "Error: `SIMPLL_VERBOSITY` must be a number!" << std::endl;
        return false;
    }

    if (level < 0) {
        std::cerr << "Error: `SIMPLL_VERBOSITY` cannot be a negative number!"
                  << std::endl;
        return false;
    }

    logger.setVerbosity(level);
    return true;
}

int main(int argc, char **argv) {
    if (!trySetVerbosity()) {
        return EINVAL;
    }

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
