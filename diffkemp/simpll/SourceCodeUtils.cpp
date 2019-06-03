//===--------- MacroUtils.cpp - Functions for working with macros ----------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains implementation of functions working with macros and their
/// differences.
///
//===----------------------------------------------------------------------===//

#include "SourceCodeUtils.h"
#include "Config.h"
#include "Utils.h"
#include <llvm/Support/Debug.h>
#include <llvm/Support/LineIterator.h>
#include <llvm/Support/raw_ostream.h>

/// Gets all macros used on the line in the form of a key to value map.
std::unordered_map<std::string, MacroElement> getAllMacrosOnLine(
    StringRef line, StringMap<MacroElement> macroMap) {
    // Transform macroMap into a second that will contain only macros that are
    // used on the line.
    // Note: For the purpose of the algorithm the starting line is treated as a
    // with a space as its key, making it an invalid identifier for a macro.
    std::unordered_map<std::string, MacroElement> usedMacroMap;
    bool mapChanged = true;
    usedMacroMap[" "] = MacroElement {"<>", line, StringRef(""), 0, ""};

    // The bodies of all macros currently present in usedMacroMap are examined;
    // every time another macro is found in it, it is added to the map.
    // This process is repeated over and over until it gets to a state when the
    // map does not change after the iteration (this means that all macros
    // already are in it).
    while (mapChanged) {
        mapChanged = false;

        // This vector is used to store the entries that are to be added to the
        // map, but can't be added directly, because it would break the cycle
        // because it is iterating over the same map.
        std::vector<std::pair<StringRef, MacroElement>> entriesToAdd;

        for (auto &Entry : usedMacroMap) {
            // Go through the macro body to find all string that could possibly
            // be macro identifiers. Because we know which characters the
            // identifier starts and ends with, we can go through the string
            // from the left, record all such substrings and test them using the
            // macro map provided in the function arguments.
            std::string macroBody = Entry.second.body.str();
            std::string potentialMacroName;

            for (int i = 0; i < macroBody.size(); i++) {
                if (potentialMacroName.size() == 0) {
                    // We are looking for the beginning of an identifier.
                    if (isValidCharForIdentifierStart(macroBody[i]))
                        potentialMacroName += macroBody[i];
                } else if (!isValidCharForIdentifier(macroBody[i]) ||
                           (i == (macroBody.size() - 1))) {
                    // We found the end of the identifier.
                    if (i == (macroBody.size() - 1) &&
                        isValidCharForIdentifier(macroBody[i]))
                        // We found the end of the entire macro - append the
                        // last character, too
                        potentialMacroName += macroBody[i];
                    auto potentialMacro = macroMap.find(potentialMacroName);
                    if (potentialMacro != macroMap.end()) {
                        // Macro used by the currently processed macro was
                        // found.
                        // Add it to entriesToAdd in order for it to be added to
                        // the map.
                        MacroElement macro = potentialMacro->second;
                        macro.parentMacro = Entry.first;
                        entriesToAdd.push_back({potentialMacro->first(),
                                                macro});
                    }

                    // Reset the potential identifier.
                    potentialMacroName = "";
                } else
                    // We are in the middle of the identifier.
                    potentialMacroName += macroBody[i];
            }
        }

        int originalMapSize = usedMacroMap.size();
        for (std::pair<StringRef, MacroElement> &Entry : entriesToAdd) {
            if (usedMacroMap.find(Entry.first.str()) == usedMacroMap.end()) {
                usedMacroMap[Entry.first.str()] = Entry.second;
                DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                dbgs() << "Adding macro " << Entry.first <<
                                " : " << Entry.second.body << ", parent macro "
                                << Entry.second.parentMacro << "\n");
            }
        }

        if (originalMapSize < usedMacroMap.size())
            mapChanged = true;
    }

    return usedMacroMap;
}

/// Extract the line corresponding to the DILocation from the C source file.
std::string extractLineFromLocation(DILocation *LineLoc) {
    // Get the path of the source file corresponding to the module where the
    // difference was found
    auto sourcePath = getSourceFilePath(
            dyn_cast<DIScope>(LineLoc->getScope()));

    // Open the C source file corresponding to the location and extract the line
    auto sourceFile = MemoryBuffer::getFile(Twine(sourcePath));
    if (sourceFile.getError()) {
        // Source file was not found, return empty string
        return "";
    }

    // Read the source file by lines, stop at the right number (the line that
    // is referenced by the DILocation)
    // The code also tries to include other lines belonging to the statement by
    // counting parenthesis - in case the line is only a part of the statement,
    // the other parts are added to it.
    line_iterator it(**sourceFile);
    std::string line, previousLine;
    while (!it.is_at_end() && it.line_number() != (LineLoc->getLine())) {
        ++it;
        if (it->count('(') < it->count(')'))
            // The line is a continuation of the previous one
            line += it->str();
        else {
            previousLine = line;
            line = it->str();
        }
    }
    if (StringRef(line).count('(') > StringRef(line).count(')')) {
        // Unfinished line
        do {
            ++it;
            line += it->str();
        } while (!it.is_at_end() && (it->count('(') < it->count(')')));
    }

    return line;
}

/// Gets all macros used on a certain DILocation in the form of a key to value
/// map.
std::unordered_map<std::string, MacroElement> getAllMacrosAtLocation(
    DILocation *LineLoc, const Module *Mod) {
    if (!LineLoc || LineLoc->getNumOperands() == 0) {
        // DILocation has no scope or is not present - cannot get macro stack
        DEBUG_WITH_TYPE(DEBUG_SIMPLL, dbgs() << "Scope for macro not found\n");
        return std::unordered_map<std::string, MacroElement>();
    }

    std::string line = extractLineFromLocation(LineLoc);
    if (line == "") {
        // Source file was not found
        DEBUG_WITH_TYPE(DEBUG_SIMPLL, dbgs() << "Source for macro not found\n");
        return std::unordered_map<std::string, MacroElement>();
    }

    DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                    dbgs() << "Looking for all macros on line:" << line
                    << "\n");

    // Get macro array from debug info
    DISubprogram *Sub = LineLoc->getScope()->getSubprogram();
    DIMacroNodeArray RawMacros = Sub->getUnit()->getMacros();

    // Create a map from macro identifiers to their values.
    StringMap<MacroElement> macroMap;
    StringMap<const DIMacroFile *> macroFileMap;
    std::vector<const DIMacroFile *> macroFileStack;

    // First all DIMacroFiles (these represent directly included headers) are
    // added to a stack.
    for (const DIMacroNode *Node : RawMacros) {
        if (const DIMacroFile *File = dyn_cast<DIMacroFile>(Node))
            macroFileStack.push_back(File);
    }

    // Next a DFS algorithm (using the stack created in the previous step) is
    // used to add all macros that are found inside the file on the top of the
    // stack to a map and to add all macro files referenced from the top macro
    // file to the stack (these represent indirectly included headers).
    while (!macroFileStack.empty()) {
        const DIMacroFile *MF = macroFileStack.back();
        DIMacroNodeArray A = MF->getElements();
        macroFileStack.pop_back();

        for (const DIMacroNode *Node : A)
            if (const DIMacroFile *File = dyn_cast<DIMacroFile>(Node))
                // The macro node is another macro file - add it to the
                // stack
                macroFileStack.push_back(File);
            else if (const DIMacro *Macro = dyn_cast<DIMacro>(Node)) {
                // The macro node is an actual macro - add an object
                // representing it (containing its full name) with its shortened
                // name as the key.
                std::string macroName = Macro->getName().str();

                // If the macro name contains arguments, remove them for the
                // purpose of the map key.
                auto position = macroName.find('(');
                if (position != std::string::npos) {
                    macroName = macroName.substr(0, position);
                }

                MacroElement element;
                element.name = macroName;
                element.body = Macro->getValue();
                element.parentMacro = "N/A";
                element.sourceFile = MF->getFile()->getFilename().str();
                element.line = Macro->getLine();
                macroMap[macroName] = element;
            }
    }

    // Add information about the original line to the map, then return the map
    auto macrosOnLine = getAllMacrosOnLine(line, macroMap);
    macrosOnLine[" "].sourceFile = getSourceFilePath(
            dyn_cast<DIScope>(LineLoc->getScope()));
    macrosOnLine[" "].line = LineLoc->getLine();

    return macrosOnLine;
}

/// Finds macro differences at the locations of the instructions L and R and
/// return them as a vector.
/// This is used when a difference is suspected to be in a macro in order to
/// include that difference into ModuleComparator, and therefore avoid an
/// empty diff.
std::vector<SyntaxDifference> findMacroDifferences(
    const Instruction *L, const Instruction *R) {
    // Try to discover a macro difference
    auto MacrosL = getAllMacrosAtLocation(L->getDebugLoc(), L->getModule());
    auto MacrosR = getAllMacrosAtLocation(R->getDebugLoc(), R->getModule());

    std::vector<SyntaxDifference> result;

    for (auto Elem : MacrosL) {
        if (Elem.second.name == "<>")
            // Note: this is the final parent "macro" element representing the
            // actual line in the source file on which the macro is used
            continue;

        auto LValue = MacrosL.find(Elem.first);
        auto RValue = MacrosR.find(Elem.first);

        if (LValue != MacrosL.end() && RValue != MacrosR.end() &&
            LValue->second.body != RValue->second.body) {
            // Macro difference found - get the macro stacks and insert the
            // object into the macro differences array to be passed on to
            // ModuleComparator.
            CallStack StackL, StackR;

            // Insert all macros between the differing macro and the original
            // macro that the line contains to the stack.
            // Note: the lines on which are the macros have to be shifted,
            // because we want the line on which the macro is used, not the line
            // on which it is defined.
            MacroElement currentElement = LValue->second;
            while (currentElement.parentMacro != "") {
                auto parent = MacrosL[currentElement.parentMacro];
                StackL.push_back(CallInfo(currentElement.name + " (macro)",
                    parent.sourceFile,
                    parent.line));
                currentElement = parent;
            }
            currentElement = RValue->second;
            while (currentElement.parentMacro != "") {
                auto parent = MacrosR[currentElement.parentMacro];
                StackR.push_back(CallInfo(currentElement.name + " (macro)",
                    parent.sourceFile,
                    parent.line));
                currentElement = parent;
            }

            // Invert stacks to match the format of actual call stacks
            std::reverse(StackL.begin(), StackL.end());
            std::reverse(StackR.begin(), StackR.end());

            DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                dbgs() << "Left stack:\n\t";
                dbgs() << LValue->second.body << "\n";
                for (CallInfo &elem : StackL) {
                    dbgs() << "\t\tfrom " << elem.fun <<
                        " in file " <<
                        elem.file << " on line "
                        << elem.line << "\n";
                });
            DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                dbgs() << "Right stack:\n\t";
                dbgs() << RValue->second.body << "\n";
                for (CallInfo &elem : StackR) {
                    dbgs() << "\t\tfrom " << elem.fun <<
                        " in file " <<
                        elem.file << " on line "
                        << elem.line << "\n";
                });

            result.push_back(SyntaxDifference {
                Elem.first, LValue->second.body, RValue->second.body,
                StackL, StackR, L->getFunction()->getName()
            });
        }
    }

    return result;
}

/// Tries to convert C source syntax of inline ASM (the input may include other
/// code, the inline asm is found and extracted) to the LLVM syntax.
/// Returns a pair of strings - the first one contains the converted ASM, the
/// second one contains (unparsed) arguments.
std::pair<std::string, std::string> convertInlineAsmToLLVMFormat(
        std::string input) {
    int position = input.find("asm");

    if (position == std::string::npos)
        // No asm present
        return {"", ""};

    // Skip characters to the first bracket
    while (input[position] != '(')
        ++position;

    // Extract the asm body
    int bracketCounter = 0;
    std::string extractedBody;
    do {
        if (input[position] == '(')
            ++bracketCounter;
        else if (input[position] == ')')
            --bracketCounter;
        extractedBody += input[position++];
    } while (bracketCounter != 0);

    // Remove the first and last bracket from the expression
    extractedBody.erase(0, 1);
    extractedBody.erase(extractedBody.size() - 2, 1);

    // Do section joining
    std::string newBody;
    std::string arguments;
    bool insideSection = false, argumentsFlag = false;
    position = 0;
    while (position < extractedBody.size()) {
        if (argumentsFlag) {
            arguments += extractedBody[position];
            ++position;
            continue;
        } else if (!insideSection && extractedBody[position] == ':') {
            argumentsFlag = true;
            continue;
        }

        if (!insideSection && extractedBody[position] == '\"')
            insideSection = true;
        else if (insideSection && extractedBody[position] != '\"')
            newBody += extractedBody[position];
        else
            insideSection = false;

        ++position;
    }

    // Replaces inline asm argument syntax (assuming there are 20 or less
    // arguments)
    for (int i = 0; i < 20; i++)
        findAndReplace(newBody, "%c" + std::to_string(i),
                       "${" + std::to_string(i) + ":c}");
    // Replace escape sequences
    findAndReplace(newBody, "\\t", "\t");
    findAndReplace(newBody, "\\n", "\n");

    return {newBody, arguments};
}

/// Takes a LLVM inline assembly with the corresponding call location and
/// retrieves the corresponding arguments in the C source code.
std::vector<std::string> findInlineAssemblySourceArguments(DILocation *LineLoc,
        const Module *Mod, std::string inlineAsm) {
    // The function searches for the inline asm at two locations - the first one
    // is the line in the original C source code corresponding to the debug info
    // location, the second one are macros used on that line.
    std::string line = extractLineFromLocation(LineLoc);
    if (line == "")
        return {};
    auto MacroMap = getAllMacrosAtLocation(LineLoc, Mod);

    std::vector<std::string> inputs;
    std::vector<std::pair<std::string, std::string>> candidates;

    // Collect all inputs in which we want to search for the inline asm.
    inputs.push_back(line);
    for (auto Elem : MacroMap)
        inputs.push_back(Elem.second.body);

    // Extract the candidates (i.e. the inputs that contain inline asm).
    for (auto input : inputs) {
        auto output = convertInlineAsmToLLVMFormat(input);

        if (output != std::pair<std::string, std::string> {"", ""})
            candidates.push_back(output);
    }

    // If there is more than one candidate, compare the candidates character by
    // character to the inline asm from LLVM IR and discards a candidate when
    // its character at the current position does not correspond. This is
    // repeated until one or no candidate is left.
    int position = 0;
    while (candidates.size() > 1) {
        std::vector<int> toErase;
        for (int i = 0; i < candidates.size(); i++) {
            if (candidates[i].first[position] != inlineAsm[position])
                toErase.push_back(i);
        }

        for (int i : toErase)
            candidates.erase(candidates.begin() + i);

        ++position;
    }

    // If there is no candidate, return empty vector
    if (candidates.size() == 0)
        return {};

    std::string rawArguments = candidates[0].second;

    // Get rid of prefix
    int counter = 0;
    while (!isValidCharForIdentifier(rawArguments[counter++]));
    rawArguments.erase(0, counter - 2);

    // Parse the arguments using a flag-controlled loop.
    // The arguments are strings inside brackets - each outermost bracket pair
    // contains one of them. The algorithm uses a bracket counter to record
    // that.
    std::vector<std::string> result;
    std::string currentResult;
    int bracketCounter = 0;
    position = 0;

    while (position < rawArguments.size()) {
        if (rawArguments[position] == '(') {
            ++bracketCounter;
            if (bracketCounter == 1) {
                // Do not record initial bracket
                ++position;
                continue;
            }
        } else if (rawArguments[position] == ')') {
            --bracketCounter;
            if (bracketCounter == 0) {
                result.push_back(currentResult);
                currentResult = "";
            }
        }

        if (bracketCounter > 0)
            currentResult += rawArguments[position];

        ++position;
    }

    return result;
}
