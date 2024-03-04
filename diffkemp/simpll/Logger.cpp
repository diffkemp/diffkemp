//===--------------- Logger.cpp - Logging debug information ---------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Kucma, xkucma00@vutbr.cz
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains functions used to log debug information.
///
//===----------------------------------------------------------------------===//

#include "Logger.h"
#include "Utils.h"
#include <llvm/Support/Debug.h>

Logger logger{};

llvm::raw_null_ostream Logger::null_stream{};

void Logger::prepLog(const char *label,
                     const BufferMessage::Value left,
                     const BufferMessage::Value right) {
    buffer.emplace_back(BufferMessage{level, label, left, right});
}

void Logger::prepContext() { level++; }

void Logger::log(bool keep, const char *force_keep_type) {
    bool force_keep =
            force_keep_type != nullptr && isCurrentDebugType(force_keep_type);
    level--;
    if (force_keep) {
        for (auto iter = buffer.rbegin(); iter != buffer.rend(); iter++) {
            if (level == iter->level) { // mark the message as force-kept
                iter->force_kept = true;
                break;
            }
            if (!keep && !iter->force_kept) { // erase non-force-kept children
                iter->label = nullptr;
                iter->force_kept = true;
            }
        }
    }
    keep = keep || force_keep;
    if (level == 0) {
        if (keep) {
            dump();
        }
        buffer.clear();
    } else if (!keep) {
        while (!buffer.empty() && buffer.back().level > level) {
            buffer.pop_back();
        }
        if (!buffer.empty()) {
            buffer.pop_back();
        }
    }
}

void Logger::dump() {
    // assuming level == 0
    for (const auto &log : buffer) {
        if (log.label == nullptr) {
            continue;
        }
        setIndent(log.level);
        LOG("L " << log.label << ": " << log.left << "\n");
        LOG("R " << log.label << ": " << log.right << "\n");
    }
    setIndent(0);
}

void Logger::setIndent(size_t target_level) {
    while (level < target_level) {
        level += 1;
        ::increaseDebugIndentLevel();
    }
    while (level > target_level) {
        level -= 1;
        ::decreaseDebugIndentLevel();
    }
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &out,
                              Logger::BufferMessage::Value value) {
    switch (value.message_type) {
    case value.llvmType:
        out << *(value.type);
        break;
    case value.llvmValue:
        // In case the value is a function, log its name instead of its body.
        if (auto *fun = dyn_cast<Function>(value.value)) {
            out << fun->getName();
        } else {
            out << *(value.value);
        }
        break;
    }
    return out;
}
