//===------------- SysctlTable.h - Linux sysctl table parsing -------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the SysctlTable class.
///
//===----------------------------------------------------------------------===//

#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <unordered_map>

using namespace llvm;

struct SysctlParam {
    const GlobalVariable *Var;
    std::vector<int> indices;
};

class SysctlTable {
  private:
    const Module *Mod;
    std::string CtlTable;
    std::unordered_map<std::string, const ConstantStruct *> SysctlMap;

    /// Get LLVM object (of type struct ctl_table) with the definition of
    /// the given sysctl option.
    const ConstantStruct *getSysctl(const std::string &SysctlName);
    /// Find sysctl with given name and get its element at the given index.
    /// The element is expected to be a global variable.
    SysctlParam getGlobalVariableAtIndex(const std::string &SysctlName,
                                         unsigned index);

  public:
    SysctlTable(const Module *Mod, const std::string &CtlTable)
            : Mod(Mod), CtlTable(CtlTable){};

    /// Parse all sysctls entries that match the given pattern. Parsed entries
    /// are LLVM objects of type "struct ctl_table" containing the sysctl
    /// definition.
    /// They are stored in SysctlMap.
    std::vector<std::string> parseSysctls(const std::string &SysctlPattern);
    /// Get the proc handler function for the given sysctl option.
    const Function *getProcFun(const std::string &SysctlName);
    /// Get the child node of the given sysctl table entry.
    SysctlParam getChild(const std::string &SysctlName);
    /// Get the data variable for the given sysctl option.
    SysctlParam getData(const std::string &SysctlName);
};
