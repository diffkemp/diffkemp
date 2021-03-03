//===------------ SysctlTable.cpp - Linux sysctl table parsing ------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains definitions of methods of the SysctlTable class.
///
//===----------------------------------------------------------------------===//

#include "SysctlTable.h"
#include "Utils.h"
#include <llvm/IR/Operator.h>
#include <sstream>
#include <vector>

bool matches(std::string Name, std::string Pattern) {
    if (Pattern == "*")
        return true;

    if (StringRef(Pattern).startswith("{")
        && StringRef(Pattern).endswith("}")) {
        std::istringstream MatchListStream;
        MatchListStream.str(Pattern.substr(1, Pattern.size() - 2));

        for (std::string line; std::getline(MatchListStream, line, '|');) {
            if (line == Name)
                return true;
        }

        return false;
    }

    return Name == Pattern;
}

/// Get LLVM object (of type struct ctl_table) with the definition of
/// the given sysctl option.
const ConstantStruct *SysctlTable::getSysctl(const std::string &SysctlName) {
    if (SysctlMap.find(SysctlName) == SysctlMap.end()) {
        parseSysctls(SysctlName);
    }

    if (SysctlMap.find(SysctlName) != SysctlMap.end())
        return SysctlMap.at(SysctlName);
    else
        return nullptr;
}

/// Parse all sysctls entries that match the given pattern. Parsed entries
/// are LLVM objects of type "struct ctl_table" containing the sysctl
/// definition.
/// They are stored in SysctlMap.
std::vector<std::string>
        SysctlTable::parseSysctls(const std::string &SysctlPattern) {
    std::vector<std::string> Result;

    // Split the sysctl pattern into components.
    std::istringstream CtlTableStream;
    CtlTableStream.str(CtlTable);

    std::vector<std::string> CtlTableParsed;
    for (std::string line; std::getline(CtlTableStream, line, '.');) {
        CtlTableParsed.push_back(line);
    }

    // Sysctl table is a global variable.
    auto TableRoot = Mod->getNamedGlobal(CtlTableParsed[0]);
    if (!TableRoot)
        return Result;

    // Get global variable initializer. If SysctlPattern contains indices,
    // follow them to get the actual table.
    auto SysctlList = TableRoot->getInitializer();
    for (int i = 1; i < CtlTableParsed.size(); i++) {
        SysctlList = dyn_cast<Constant>(
                SysctlList->getOperand(std::stoi(CtlTableParsed[i])));
    }
    if (!SysctlList)
        return Result;

    /// Iterate all entries in the table.
    for (int i = 0; i < SysctlList->getNumOperands(); i++) {
        auto Sysctl = dyn_cast<ConstantStruct>(SysctlList->getOperand(i));
        if (!Sysctl || Sysctl->getNumOperands() == 0)
            continue;

        // Sysctl option name is the first element of the entry.
        auto GEP = dyn_cast<GEPOperator>(Sysctl->getOperand(0));
        if (!GEP)
            continue;

        // Sysctl name is a constant string stored inside a global variable.
        auto GVar = dyn_cast<GlobalVariable>(GEP->getOperand(0));
        if (!GVar)
            continue;
        auto StringConst =
                dyn_cast<ConstantDataSequential>(GVar->getInitializer());
        if (!StringConst || !StringConst->isString())
            continue;
        // Remove \0 from the end of the string.
        std::string Name = StringConst->getAsString().rtrim('\0').str();

        // Parse sysctl pattern.
        std::istringstream SysctlPatternStream;
        SysctlPatternStream.str(SysctlPattern);

        std::vector<std::string> SysctlPatternParsed;
        for (std::string line; std::getline(SysctlPatternStream, line, '.');) {
            SysctlPatternParsed.push_back(line);
        }

        // Look whether the pattern matches the sysctl name.
        std::string Pattern =
                SysctlPatternParsed[SysctlPatternParsed.size() - 1];
        if (matches(Name, Pattern)) {
            std::string SysctlName = SysctlPattern;
            findAndReplace(SysctlName, Pattern, Name);
            SysctlMap[SysctlName] = Sysctl;
            Result.push_back(SysctlName);
        }
    }

    return Result;
}

/// Find sysctl with given name and get its element at the given index.
SysctlParam SysctlTable::getGlobalVariableAtIndex(const std::string &SysctlName,
                                                  unsigned index) {
    SysctlParam Result{nullptr, {}};

    // Get the sysctl entry.
    auto Sysctl = getSysctl(SysctlName);
    if (!Sysctl || Sysctl->getNumOperands() <= index)
        return Result;

    // Get operand at the given index.
    auto Data = Sysctl->getOperand(index);
    if (!Data)
        return Result;

    if (auto GEP = dyn_cast<GEPOperator>(Data)) {
        // Address is a GEP, we have to extract the actual variable.
        for (int i = 1; i < Data->getNumOperands(); i++) {
            if (auto Const = dyn_cast<ConstantInt>(Data->getOperand(i))) {
                Result.indices.push_back(Const->getZExtValue());
            } else {
                // Non-constant indices are unsupported.
                return {nullptr, {}};
            }
        }

        Data = dyn_cast<Constant>(GEP->getOperand(0));
    }

    if (auto Cast = dyn_cast<BitCastOperator>(Data)) {
        // Address is typed to "void *", we need to get rid of the bitcast
        // operator.
        Data = dyn_cast<Constant>(Cast->getOperand(0));
    }

    if (auto GV = dyn_cast<GlobalVariable>(Data)) {
        Result.Var = GV;
        return Result;
    }

    return {nullptr, {}};
}

/// Get the proc handler function for the given sysctl option.
const Function *SysctlTable::getProcFun(const std::string &SysctlName) {
    auto Sysctl = getSysctl(SysctlName);
    if (!Sysctl || Sysctl->getNumOperands() < 6)
        return nullptr;

    // Proc handler function is the 6th element of the "struct ctl_table" type.
    return dyn_cast<Function>(Sysctl->getOperand(5));
}

/// Get the child node of the given sysctl table entry.
SysctlParam SysctlTable::getChild(const std::string &SysctlName) {
    return getGlobalVariableAtIndex(SysctlName, 4);
}

/// Get name of the data variable for the given sysctl option.
SysctlParam SysctlTable::getData(const std::string &SysctlName) {
    return getGlobalVariableAtIndex(SysctlName, 1);
}
