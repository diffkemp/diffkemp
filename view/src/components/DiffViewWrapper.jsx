// Wrapper of react-diff-view package for showing code of functions
// Author: Lukas Petr

import {
  useCallback, useEffect, useMemo, useState,
} from 'react';
import { PropTypes } from 'prop-types';
import {
  Diff,
  Hunk,
  parseDiff,
  expandFromRawCode,
  Decoration,
  textLinesToHunk,
  tokenize,
  findChangeByOldLineNumber,
  findChangeByNewLineNumber,
  getChangeKey,
  getCorrespondingOldLineNumber,
} from 'react-diff-view';
import refractor from 'refractor';

/** Show whole function if function has max this amount of line. */
const MAX_LINES_THRESHOLD = 25;
/** Amount of lines to show around calling function
 * if function is too long to show whole. */
const LINES_OF_CONTEXT = 10;

function isFunctionLong(startLine, endLine) {
  return endLine - startLine >= MAX_LINES_THRESHOLD;
}

/**
 * Creates a file structure necessary for visualisation of two functions
 * with empty diff. Creates one hunk containing just a single line:
 * the line numbered oldStart in oldCode. newStart is the number of
 * the corresponding line in the new file.
 * @param {String} oldCode - Content of file containing old version of function
 * @returns Returns file structure containing created hunk.
 */
function createFileStructure(oldCode, oldStart, newStart) {
  const linesOfCode = oldCode.split('\n');
  const lineOfCode = linesOfCode[oldStart - 1];
  const hunkFromLine = textLinesToHunk([lineOfCode], oldStart, newStart);
  const file = {
    type: 'modify',
    hunks: [hunkFromLine],
  };
  return file;
}

/**
 * Transform lines of code (to be highlighted) to array which Diff accepts.
 * @param {Number} linesToShow - Lines (old, new) of code which should be highlighted.
 * @returns Returns array of lines to be highlighted acceptable by Diff.
 */
function getLinesToHighlight(linesToShow, hunks) {
  if (linesToShow) {
    const oldChange = findChangeByOldLineNumber(hunks, linesToShow[0]);
    const newChange = findChangeByNewLineNumber(hunks, linesToShow[1]);
    if (oldChange && newChange) {
      const oldChangeKey = getChangeKey(oldChange);
      const newChangeKey = getChangeKey(newChange);
      if (oldChangeKey === newChangeKey) return [oldChangeKey];
      return [oldChangeKey, newChangeKey];
    }
  }
  return [];
}

/**
 * Creates a file structure for code visualisation of one version of a symbol.
 * @param {boolean} isOld - True if the version is old version of the symbol, otherwise false.
 * @param {string} code - Content of a file in which is the symbol located.
 * @param {number} start - Line on which the code of the symbol starts.
 * @param {number} end - Line on which the code of the symbol ends.
 * @returns Returns file structure containing code of the symbol.
 */
function createFileStructForOneSide(isOld, code, start, end) {
  // Note: The structure which react-diff-view uses
  // is described here https://github.com/ecomfe/gitdiff-parser#api
  // (gitdiff-parser is a package which react-diff-view uses).
  let lines = code.split('\n');
  lines = lines.slice(start - 1, end);
  // Creating changes from every line. Based on whether the code belongs to the old or new version,
  // setting the change type so the code is shown on the correct side.
  const changes = lines.map((line, index) => ({
    content: line,
    type: isOld ? 'delete' : 'insert',
    lineNumber: start + index,
    isDelete: isOld,
    isInsert: !isOld,
  }));
  // Creating hunk with the changes and setting attributes based on which version
  // the code belongs to.
  const hunk = {
    oldStart: isOld ? start : 0,
    oldLines: isOld ? end - start + 1 : 0,
    newStart: isOld ? 0 : start,
    newLines: isOld ? 0 : end - start + 1,
    changes,
  };
  hunk.content = `@@ -${hunk.oldStart},${hunk.oldLines} +${hunk.newStart},${hunk.newLines} @@`;
  // Note: Missing fields: oldMode, newMode, similarity, oldRevision, newRevision,
  // Note 2: `type` must be 'modify' so the code is shown on the correct side.
  const file = {
    hunks: [hunk],
    oldEndingNewLine: true,
    newEndingNewLine: true,
    oldPath: '',
    newPath: '',
    type: 'modify',
  };
  return file;
}

function getSyntaxHighlightedTokens(hunks) {
  const options = {
    highlight: true,
    refractor,
    language: 'c',
  };
  return tokenize(hunks, options);
}

/**
 * A component for visualisation of source code of two versions of a function.
 * Function refers generally to symbol definition, including functions, types and macros.
 *
 * For the function, we show:
 * - lines containing differences,
 * - the first and the last line of the function.
 *
 * If the function is calling another function (`linesToShow` is set), then
 * - if the function is short (<= MAX_LINES_THRESHOLD), the entire function is shown,
 * - otherwise, only the line with the call surrounded by a certain
 *   amount (LINES_OF_CONTEXT) of lines is shown.
 *
 * It is possible to show source code of only one version (old/new) of a function
 * in that case set the code of the other version (oldCode/newCode) to empty string.
 * The entire code of the function is shown then.
 *
 * For the lines which are not shown, we render buttons that allow user to show these lines.
 * @param {Object} props - The old/new props can be null when showing only one version
 *   of the function.
 * @param {string} props.oldCode - Source code of file containing old version of the function.
 * @param {string} props.newCode - Source code of file containing new version of the function.
 *   If oldCode or newCode is empty string, then we are showing only one version of function.
 * @param {string} props.diff - Output of unified diff of old and new source code.
 * @param {number} props.oldStart - Line where the old version of the function starts.
 * @param {number} props.newStart - Line where the new version of the function starts.
 * @param {number} props.oldEnd - Line where the old version of the function ends.
 * @param {number} props.newEnd - Line where the new version of the function ends.
 * @param {Boolean} [props.showDiff=true] - True to color differences in function.
 * @param {Number} [props.linesToShow=null] - Lines (old, new) of function which
 *                                            should be shown and highlighted.
 */
export default function DiffViewWrapper({
  oldCode,
  newCode,
  diff,
  oldStart,
  newStart,
  oldEnd,
  newEnd,
  showDiff = true,
  linesToShow = null,
}) {
  /* Showing code only for one version of a function? */
  const onlyOneSide = (oldCode === '' || newCode === '');
  /**
   * Getting object which Diff component needs, it is an object which represents
   * differences in a file, it contains hunks (group of changes),
   * the hunks contain individual line changes (insert/delete/normal).
   */
  const file = useMemo(() => {
    if (onlyOneSide) {
      // Only one side/version of code will be shown - creating the object from the code.
      const onlyOld = (newCode === '');
      const [code, start, end] = onlyOld
        ? [oldCode, oldStart, oldEnd]
        : [newCode, newStart, newEnd];
      return createFileStructForOneSide(onlyOld, code, start, end);
    }
    if (diff !== '') {
      // Code contains syntax differences - creating the object from the diff.
      return parseDiff(diff)[0];
    }
    // Code does not contain syntax differences - creating the object using first line of code.
    return createFileStructure(oldCode, oldStart, newStart);
  }, [diff, oldCode, newCode, oldStart, newStart, oldEnd, newEnd, onlyOneSide]);
  /* hunks of differences/code which will be shown */
  const [hunks, setHunks] = useState(null);
  // Function for adding lines of old code from startLine to endLine
  // which should be shown.
  // TODO - edit to work also if showing only one version of a function.
  // Currently it does not make sense, because only one version is shown
  // in case of a macro call, for macro calls we do not know on which line
  // is used next macro and we are showing whole body of macro.
  const expandHunks = useCallback(
    (startLine, endLine = startLine) => {
      setHunks((oldHunks) => expandFromRawCode(oldHunks, oldCode, startLine, endLine + 1));
    },
    [oldCode],
  );

  /**
   * Setting hunks (lines) which will be shown.
   * See the component description comment for information on which lines are shown.
   */
  useEffect(() => {
    const addLineWithContext = (line) => {
      const beginning = Math.max(line - LINES_OF_CONTEXT, oldStart);
      const ending = Math.min(line + LINES_OF_CONTEXT, oldEnd);
      expandHunks(beginning, ending);
    };

    const addAllLinesOfFunction = () => {
      expandHunks(oldStart, oldEnd);
    };
    // filters out diff hunks which are not located in selected code
    const getHunksInRange = (oldHunks) => oldHunks.filter(
      (hunk) => hunk.oldStart <= oldEnd
                    && hunk.oldStart + hunk.oldLines >= oldStart,
    );
    const addFirstAndLastLine = () => {
      expandHunks(oldStart);
      expandHunks(oldEnd);
    };

    setHunks(file.hunks);
    // If showing code for onlyOneSide, then right now we are showing the whole function.
    if (onlyOneSide) return;
    addFirstAndLastLine();
    setHunks((oldHunks) => getHunksInRange(oldHunks));
    // Showing function which is called instead of diff of function
    if (linesToShow) {
      if (isFunctionLong(oldStart, oldEnd)) {
        // old calling line
        addLineWithContext(linesToShow[0]);
        // add new calling line in case if it is in different change
        // than old calling line
        const oldLineOfNewCalling = getCorrespondingOldLineNumber(
          file.hunks,
          linesToShow[1],
        );
        if (oldLineOfNewCalling !== linesToShow[0]) {
          addLineWithContext(oldLineOfNewCalling);
        }
      } else {
        addAllLinesOfFunction();
      }
    }
  }, [file, oldCode, oldStart, oldEnd, linesToShow, expandHunks, onlyOneSide]);

  /**
   * Function for rendering/adding expand button on places
   * where are missing lines of code of function.
   */
  const renderHunks = (output, currentHunk, index, allHunks) => {
    const previousHunk = index > 0 ? allHunks[index - 1] : null;

    if (previousHunk) {
      const nextStart = previousHunk.oldStart + previousHunk.oldLines;
      if (nextStart < currentHunk.oldStart) {
        output.push(
          <Decoration key={`expand${currentHunk.content}`}>
            <button
              className="diff-view-expand-button"
              type="button"
              onClick={() => expandHunks(nextStart, currentHunk.oldStart - 1)}
            >
              ↕ Expand lines
            </button>
          </Decoration>,
        );
      }
    }
    output.push(<Hunk hunk={currentHunk} key={currentHunk.content} />);
    return output;
  };

  if (!hunks) {
    return <>Loading...</>;
  }

  return (
    <Diff
      viewType="split"
      hunks={hunks}
      diffType={file.type}
      className={showDiff ? 'show-diff' : 'do-not-show-diff'}
      selectedChanges={getLinesToHighlight(linesToShow, hunks)}
      tokens={getSyntaxHighlightedTokens(hunks)}
    >
      {(arrayOfHunks) => arrayOfHunks.reduce(renderHunks, [])}
    </Diff>
  );
}

DiffViewWrapper.propTypes = {
  oldCode: PropTypes.string,
  newCode: PropTypes.string,
  diff: PropTypes.string.isRequired,
  oldStart: PropTypes.number,
  newStart: PropTypes.number,
  oldEnd: PropTypes.number,
  newEnd: PropTypes.number,
  showDiff: PropTypes.bool,
  linesToShow: PropTypes.arrayOf(PropTypes.number),
};
