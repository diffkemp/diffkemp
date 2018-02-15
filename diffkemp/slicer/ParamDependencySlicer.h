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

    // We only do the slicing if the function uses the parameter
    bool uses_param = false;

    // Functions for adding to sets
    void addAllInstrs(const std::vector<const BasicBlock *> BBs);
    bool addToSet(const Instruction *Inst,
                  std::set<const Instruction *> &set);
    bool addToDependent(const Instruction *Instr);
    bool addToIncluded(const Instruction *Inst);
    bool addAllOpsToIncluded(const Instruction *Inst);

    // Functions for searching sets
    inline bool isDependent(const Instruction *Instr);
    inline bool isIncluded(const Instruction *Instr);
    inline bool isAffected(const BasicBlock *BB);
    inline bool isIncluded(const BasicBlock *BB);

    // Computing affected and included basic blocks
    auto affectedBasicBlocks(BranchInst *Branch)
        -> std::vector<const BasicBlock *>;

    auto includedSuccessors(TerminatorInst &Terminator,
                            const BasicBlock *ExitBlock)
        -> std::set<BasicBlock *>;

    // Computing reachable blocks
    auto reachableBlocks(const BasicBlock *Src, Function &Fun)
        -> std::set<const BasicBlock *>;

    auto reachableBlocksThroughSucc(TerminatorInst *Terminator,
                                    BasicBlock *Succ)
        -> std::set<const BasicBlock *>;

    // Set operations
    void intersectWith(std::set<const BasicBlock *> &set,
                       const std::set<const BasicBlock *> &other);
    void uniteWith(std::set<const BasicBlock *> &set,
                   const std::set<const BasicBlock *> &other);

    bool checkDependency(const Use *Op);

    void mockReturn(BasicBlock *ReturnBB, Type *RetType);

    bool canRemoveBlock(const BasicBlock *bb);
    bool canRemoveFirstBlock(const BasicBlock *bb);

    static bool isLlreveIntrinsic(const Function &f);
};

// Register the pass
static RegisterPass<ParamDependencySlicer> X("paramdep-slicer",
                                             "Parameter Depependency Slicer");
