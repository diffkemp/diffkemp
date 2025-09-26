//===---------------- Logger.h - Logging debug information ----------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Kucma, xkucma00@vutbr.cz
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the Logger class that provides
/// debug logging functions.
///
//===----------------------------------------------------------------------===//

/**
 * Usage:
 *
 * The standard use case of this logger is using LOG(message) macro to log
 * messages to the debug output, which happens only if the debug output is
 * enabled (DEBUG_SIMPLL flag). Multiple strings or printable objects can be
 * chained within single LOG using operator '<<', for example:
 *
 *  LOG("Value: " << LeftValue << "\n").
 *
 * Each LOG automatically adds indentation to the beginning of the message,
 * determined by the current indentation level, which can be manipulated by
 * the macros LOG_INDENT() and LOG_UNINDENT(). This allows to log messages in
 * a hierarchical manner. To log messages without indentation (e.g. when adding
 * a single '\n' to the end of an already printed line), add suffix _NO_INDENT
 * to the LOG macro.
 *
 * LOG_VERBOSE and LOG_VERBOSE_EXTRA (and their _NO_INDENT variants) behave
 * analogically to LOG, but require setting a higher level of debug to print
 * their messages (DEBUG_SIMPLL_VERBOSE and DEBUG_SIMPLL_VERBOSE_EXTRA,
 * respectively; for their meaning and usage, see 'Debug levels' below).
 *
 *
 * Logging hierarchical comparisons in 'DifferentialFunctionComparator.cpp'
 * requires a special behavior for two reasons. By default, it's desirable to
 * only log the comparisons where a difference was found in it and all its
 * predecessors. Thus, logging needs to be done conditionally, based on the
 * result of the comparison. Additionally, when a comparison is finished, the
 * results of its predecessors are not yet known, and so the log of the
 * comparison must be stored until its predecessors are resolved.
 *
 * Comparison log is first prepared using macro PREP_LOG(label, left, right) at
 * the beginning of the comparison function, where 'label' is a string
 * describing the compared objects and 'left'/'right' are pointers to the
 * compared objects. This macro stores a representation of the comparison
 * message in a buffer and prepares the context for logging its children.
 * Example of usage:
 *
 *  PREP_LOG("value", L, R);
 *
 * Then, instead of calling return, RETURN_WITH_LOG(return_value) is used,
 * ending the context. The result of the comparison is used to determine whether
 * to keep the message and all its children, alternatively erasing them in the
 * case no difference was found. Example of usage:
 *
 *  if (int Res = cmpValues(CL->getArgOperand(i), CR->getArgOperand(i)))
 *      RETURN_WITH_LOG(Res);
 *
 * At the higher levels of debug, all comparisons are stored, regardless of the
 * result. For the cases where this isn't desired, a variant of this macro
 * RETURN_WITH_LOG_NEQ(return_value) can be used, ensuring keeping the message
 * only if a difference was found in it and all its predecessors, regardless of
 * the currently configured debug level.
 *
 * If RETURN_WITH_LOG (or its _NEQ variant) executed in the lowest level
 * comparison leads to keeping the logged message, it and all its stored
 * children comparison logs will be additionally automatically printed to the
 * debug output. Base indentation for each line is determined by the current
 * indentation level. Additional indentation is added to the individual messages
 * based on their heirarchy.
 *
 *
 * Debug levels (in ascending order):
 *  - DEBUG_SIMPLL logs:
 *      - module preprocessing
 *      - function comparisons
 *      - passes
 *      - relocations
 *      - pattern sets and pattern comparisons
 *      - LLVM debug information analysis
 * - DEBUG_SIMPLL_VERBOSE additionally logs:
 *      - comparisons where a difference was found
 * - DEBUG_SIMPLL_VERBOSE_EXTRA additionally logs:
 *      - comparisons where no difference was found (unless _NEQ was used)
 *      - macro processing
 *      - details about index alignment in debug information analysis
 *      - details about replacements in passes
 *      - details about dependency slicing pass
 *      - details about inverse conditions pattern
 */

#ifndef DIFFKEMP_SIMPLL_LOGGER_H
#define DIFFKEMP_SIMPLL_LOGGER_H

#include <Config.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <vector>

// Definition of debug types (used in llvm::setCurrentDebugTypes)
#define DEBUG_SIMPLL "debug-simpll"
#define DEBUG_SIMPLL_VERBOSE "debug-simpll-verbose"
#define DEBUG_SIMPLL_VERBOSE_EXTRA "debug-simpll-verbose-extra"

#define LOGGER_BASE_LEVEL DEBUG_SIMPLL_VERBOSE
#define LOGGER_FORCE_LEVEL DEBUG_SIMPLL_VERBOSE_EXTRA

// Checks if given logger level is turned on
#define IS_LOG_VERBOSE_ON() isCurrentDebugType(LOGGER_BASE_LEVEL)
#define IS_LOG_VERBOSE_EXTRA_ON() isCurrentDebugType(LOGGER_FORCE_LEVEL)

// Temporarily turns off the logger if it is turned on.
#define LOG_OFF()                                                              \
    do {                                                                       \
        if (DebugFlag) {                                                       \
            DebugFlag = false;                                                 \
            logger.off = true;                                                 \
        }                                                                      \
    } while (false)

// Temporarily turns off the logger if the level is not LOGGER_FORCE_LEVEL.
// If it is then nothing happens.
#define LOG_OFF_FOR_NO_FORCE()                                                 \
    do {                                                                       \
        if (!IS_LOG_VERBOSE_EXTRA_ON()) {                                      \
            LOG_OFF();                                                         \
        }                                                                      \
    } while (false)

// Turns on the logger if it was previously temporarily turned off by LOG_OFF
// or LOG_OFF_FOR_NO_FORCE macros.
#define LOG_ON()                                                               \
    do {                                                                       \
        if (logger.off) {                                                      \
            DebugFlag = true;                                                  \
            logger.off = false;                                                \
        }                                                                      \
    } while (false)

// Prepare a log message for (potential) future logging and create context for
// logging its children. Must be later followed by LOG_KEEP or RETURN_WITH_LOG
// (or the _FORCE/_NEQ variant, respectively), determining whether to keep
// the message and marking the end of the context (see below for difference
// between the variants).
// Called once per each comparison level.
#define PREP_LOG(label, left, right)                                           \
    DEBUG_WITH_TYPE(LOGGER_BASE_LEVEL, logger.prepLog(label, left, right);     \
                    logger.prepContext();)

// Based on the given value (interpreted as bool), either keep or erase the
// prepared message and its children. Additionally, if the prepared message
// is kept and has no parent (meaning it's the lowest level message), print and
// remove all stored messages, clearing the buffer.
#define LOG_KEEP(keep) DEBUG_WITH_TYPE(LOGGER_BASE_LEVEL, logger.log(keep))

// Force variant of LOG_KEEP.
// If LOGGER_FORCE_LEVEL is not enabled, behaves identically to LOG_KEEP.
// If it is enabled, forces keeping the message regardless of the given value
// and marks that message as force-kept. However, its children that weren't
// previously marked as force-kept will still be erased if the given value is
// evaluated to false.
#define LOG_KEEP_FORCE(keep)                                                   \
    DEBUG_WITH_TYPE(LOGGER_BASE_LEVEL, logger.log(keep, LOGGER_FORCE_LEVEL))

// Return and in case the return value is zero, i.e. no difference was found
// in the current cmp* function, erase the prepared log message and its
// children, ensuring only comparisons with difference are logged.
// If LOGGER_FORCE_LEVEL is enabled, keeps the message regardless of the given
// value and marks that message as force-kept. However, if the return value is
// zero, non-force-kept children of a force-kept message are still erased.
// Used for comparisons where it is desirable to log that a comparison has
// occured even if no difference was found (when using higher level of debug
// verbosity).
#define RETURN_WITH_LOG(return_value)                                          \
    do {                                                                       \
        const auto &x = return_value;                                          \
        LOG_KEEP_FORCE(x);                                                     \
        return x;                                                              \
    } while (false)

// Variant of RETURN_WITH_LOG that only logs the comparisons where a difference
// was found, regardless of the configured debug level. Messages logged using
// this macro are never marked as force-kept.
// Used for less important comparisons.
#define RETURN_WITH_LOG_NEQ(return_value)                                      \
    do {                                                                       \
        const auto &x = return_value;                                          \
        LOG_KEEP(x);                                                           \
        return x;                                                              \
    } while (false)

#define LOG_NO_INDENT(msg) DEBUG_WITH_TYPE(DEBUG_SIMPLL, dbgs() << msg)
#define LOG_VERBOSE_NO_INDENT(msg)                                             \
    DEBUG_WITH_TYPE(DEBUG_SIMPLL_VERBOSE, dbgs() << msg)
#define LOG_VERBOSE_EXTRA_NO_INDENT(msg)                                       \
    DEBUG_WITH_TYPE(DEBUG_SIMPLL_VERBOSE_EXTRA, dbgs() << msg)

#define LOG(msg) LOG_NO_INDENT(getDebugIndent() << msg)
#define LOG_VERBOSE(msg) LOG_VERBOSE_NO_INDENT(getDebugIndent() << msg)
#define LOG_VERBOSE_EXTRA(msg)                                                 \
    LOG_VERBOSE_EXTRA_NO_INDENT(getDebugIndent() << msg)

#define LOG_INDENT() DEBUG_WITH_TYPE(DEBUG_SIMPLL, increaseDebugIndentLevel())
#define LOG_UNINDENT() DEBUG_WITH_TYPE(DEBUG_SIMPLL, decreaseDebugIndentLevel())

class Logger {
  public:
    // Flag indicating if the logger is temporary turned off by LOG_OFF or
    // LOG_OFF_FOR_NO_FORCE macros.
    bool off = false;
    struct BufferMessage {
        struct Value {
            enum {
                llvmValue,
                llvmType,
            } message_type;
            union {
                const llvm::Value *value;
                const llvm::Type *type;
            };
            Value(const llvm::Value *value)
                    : message_type{llvmValue}, value{value} {};
            Value(const llvm::Type *type)
                    : message_type{llvmType}, type{type} {};
        };
        bool force_kept{false};
        size_t level;
        const char *label;
        const Value left;
        const Value right;
        BufferMessage(size_t level,
                      const char *label,
                      const Value left,
                      const Value right)
                : level{level}, label{label}, left{left}, right{right} {};
    };
    Logger(){};
    // prepare message for logging
    void prepLog(const char *label,
                 const class BufferMessage::Value left,
                 const class BufferMessage::Value right);
    // prepare for logging messages within the context of the last prepared
    // message
    void prepContext();
    // log a prepared message
    void log(bool keep = true, const char *force_keep_type = nullptr);
    // dump all messages from the buffer
    void dump();
    // sets logger verbosity level
    void setVerbosity(unsigned level);

  protected:
    // set the logger indentation level to the given value, while modifying the
    // real debug indentation as well
    void setIndent(size_t target_level);

  private:
    // current level of indentation within the logger
    size_t level{0};
    // debug message buffer (force-kept, indent level, message)
    std::vector<BufferMessage> buffer{};
    // stream used to append to messages in buffer
    std::unique_ptr<llvm::raw_ostream> stream{};
    // null stream used for unwanted messages
    static llvm::raw_null_ostream null_stream;
    // sets debug types specified in the vector
    void setDebugTypes(const std::vector<std::string> &debugTypes);
};

llvm::raw_ostream &operator<<(llvm::raw_ostream &out,
                              Logger::BufferMessage::Value value);

extern Logger logger;

#endif // DIFFKEMP_SIMPLL_LOGGER_H
