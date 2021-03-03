//===-------------------- Result.h - Comparison result --------------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains definitions of methods for classes representing function
/// comparison result.
///
//===----------------------------------------------------------------------===//

#include "Result.h"
#include "Utils.h"

Result::Result(Function *FirstFun, Function *SecondFun)
        : First(FirstFun->getName().str(),
                getFileForFun(FirstFun),
                FirstFun->getSubprogram() ? FirstFun->getSubprogram()->getLine()
                                          : 0),
          Second(SecondFun->getName().str(),
                 getFileForFun(SecondFun),
                 SecondFun->getSubprogram()
                         ? SecondFun->getSubprogram()->getLine()
                         : 0) {}

/// Add new differing object.
void Result::addDifferingObject(std::unique_ptr<NonFunctionDifference> Object) {
    DifferingObjects.push_back(std::move(Object));
}

/// Add multiple SyntaxDifference objects.
void Result::addDifferingObjects(
        std::vector<std::unique_ptr<SyntaxDifference>> &&Objects) {
    DifferingObjects.insert(DifferingObjects.end(),
                            std::make_move_iterator(Objects.begin()),
                            std::make_move_iterator(Objects.end()));
}

/// Add multiple TypeDifference objects.
void Result::addDifferingObjects(
        std::vector<std::unique_ptr<TypeDifference>> &&Objects) {
    DifferingObjects.insert(DifferingObjects.end(),
                            std::make_move_iterator(Objects.begin()),
                            std::make_move_iterator(Objects.end()));
}
