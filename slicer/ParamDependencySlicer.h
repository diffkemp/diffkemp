/*
 * Created by Viktor Malik (vmalik@redhat.com)
 *
 * Published under Apache 2.0 license.
 * See LICENSE for details.
 */

#pragma once

#include <set>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Instructions.h>

using namespace llvm;

static cl::opt<std::string> ParamName("param-name",
                                      cl::Hidden,
                                      cl::desc("Parameter name"));

/*
 * Pass implementing a slicer that removes everything that is not dependent on
 * the parameter passed as command line option.
 */
class ParamDependencySlicer : public FunctionPass {
public:
    static char ID;

    ParamDependencySlicer() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F) override;

    virtual void getAnalysisUsage(AnalysisUsage &usage) const override;

private:
    // Instructions directly dependent on the parameter
    std::set<const Instruction *> DependentInstrs = {};
    // Instructions that must be included
    std::set<const Instruction *> IncludedInstrs = {};
    // Basics blocks whose execution is dependent on the parameter
    std::set<const BasicBlock *> AffectedBasicBlocks = {};
    // Basic blocks that must be included
    std::set<const BasicBlock *> IncludedBasicBlocks = {};
    // Mapping block to its successor (for single-successor blocks only)
    std::map<const BasicBlock *, BasicBlock *> SuccessorsMap = {};

    // Functions for adding to sets
    void addAllInstrs(const std::vector<const BasicBlock *> BBs);
    bool addToSet(const Instruction *Inst,
                  std::set<const Instruction *> &set);
    bool addToDependent(const Instruction *Instr);
    bool addToIncluded(const Instruction *Inst);
    bool addAllOpsToIncluded(const Instruction *Inst);

    // Computing affected and included basic blocks
    auto affectedBasicBlocks(const BranchInst &Branch,
                             const Function &Fun)
        -> std::vector<const BasicBlock *>;
    auto includedSuccessors(const TerminatorInst &Terminator,
                            const BasicBlock *ExitBlock)
        -> std::set<BasicBlock *>;

    bool checkDependency(const Use *Op);

    void mockReturn(BasicBlock *ReturnBB, Type *RetType);

    void clearOperand(Instruction &Inst, unsigned index);
    void clearArgOperand(CallInst &Inst, unsigned index);

    static bool isLlreveIntrinsic(const Function &f);
};

// Register the pass
static RegisterPass<ParamDependencySlicer> X("paramdep-slicer",
                                             "Parameter Depependency Slicer");
