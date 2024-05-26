// Component for visualisation of found difference
// Author: Lukas Petr

import { useMemo, useState } from 'react';
import { PropTypes } from 'prop-types';

import Container from 'react-bootstrap/Container';
import Col from 'react-bootstrap/Col';
import Row from 'react-bootstrap/Row';
import Alert from 'react-bootstrap/Alert';

import Callstack, { CallSide } from './Callstack';
import Code from './Code';
import { DefinitionsPropTypes, DiffPropTypes } from '../PropTypesValues';

/**
 * @param {Object} props
 * @param {string} props.compare Name of compared function.
 * @param {Object} props.diff Part of yaml containing info about difference.
 * @param {string} props.oldFolder Name of old folder (only name of subdir).
 * @param {string} props.newFolder Name of new folder (only name of subdir).
 * @param {Object} props.definition Part of yaml containing definitions.
 * @param {Function} props.getFile Function for getting content of files.
 */
export default function Difference({
  compare,
  diff,
  oldFolder,
  newFolder,
  definitions,
  getFile,
}) {
  // Name (with kind) of differing symbol.
  const [oldDiffering, newDiffering] = useMemo(() => {
    let oldD;
    let newD;
    if (diff['old-callstack'].length > 0) {
      oldD = diff['old-callstack'][diff['old-callstack'].length - 1].name;
      newD = diff['new-callstack'][diff['new-callstack'].length - 1].name;
    } else {
      // If there is no call stack, the differing function is the compared function.
      oldD = compare;
      newD = compare;
    }
    return [oldD, newD];
  }, [diff, compare]);
  // Function which code to show (defaultly showing the differing function).
  const [functionToShow, setFunctionToShow] = useState({
    name: oldDiffering,
    // The differing function should be located in both call stacks.
    side: CallSide.BOTH,
  });
  let codeBlock = null;
  let errorCodeMessage = null;
  // extracting only name (putting away kind)
  const name = functionToShow.name.split(' ')[0];
  // Extracting on which side is the call located.
  const { side } = functionToShow;

  const needsOldCode = (side === CallSide.BOTH || side === CallSide.OLD);
  const needsNewCode = (side === CallSide.BOTH || side === CallSide.NEW);
  const containsDefinition = (name in definitions);
  const containsOldDef = containsDefinition && 'old' in definitions[name];
  const containsNewDef = containsDefinition && 'new' in definitions[name];
  // If we need old/new code then it has to contain old/new defintion.
  // Note: implication X => Y <=> not X or Y
  if ((!needsOldCode || containsOldDef) && (!needsNewCode || containsNewDef)) {
    const oldDefintion = definitions[name]?.old;
    const newDefintion = definitions[name]?.new;

    // returns line where is called next function in callstack
    // or undefined if it is differing function
    const getCallingLine = () => {
      // indexes to callstack containing called function
      let oldIndex = 0;
      let newIndex = 0;
      if (functionToShow.name !== compare) {
        oldIndex = diff['old-callstack'].findIndex(
          (call) => call.name === functionToShow.name,
        ) + 1;
        newIndex = diff['new-callstack'].findIndex(
          (call) => call.name === functionToShow.name,
        ) + 1;
      }

      if (
        diff['old-callstack'].length > oldIndex
                && diff['new-callstack'].length > newIndex
      ) {
        return [
          diff['old-callstack'][oldIndex].line,
          diff['new-callstack'][newIndex].line,
        ];
      }
      return undefined;
    };
    if ((needsOldCode && !('end-line' in oldDefintion))
        || (needsNewCode && !('end-line' in newDefintion))) {
      errorCodeMessage = 'Missing info about ending of function.';
    } else {
      // creating component for code visualisation
      // if all information are included for showing code of function
      const specification = {
        oldSrc: needsOldCode ? oldDefintion.file : null,
        newSrc: needsNewCode ? newDefintion.file : null,
        diff: side === CallSide.BOTH && definitions[name].diff
          ? `${name}.diff`
          : null,
        oldStart: needsOldCode ? oldDefintion.line : null,
        newStart: needsNewCode ? newDefintion.line : null,
        oldEnd: needsOldCode ? oldDefintion['end-line'] : null,
        newEnd: needsNewCode ? newDefintion['end-line'] : null,
        differing: [oldDiffering, newDiffering].includes(functionToShow.name),
      };
      // If the selected symbol is a function, get the line on which is called
      // next symbol from the call stack.
      if (definitions[name].kind === 'function') {
        specification.calling = getCallingLine();
      }
      codeBlock = (
        <Code
          key={`${compare}-${name}`}
          specification={specification}
          getFile={getFile}
          oldFolder={oldFolder}
          newFolder={newFolder}
        />
      );
    }
  } else {
    errorCodeMessage = 'Missing info about definitions of functions.';
  }

  return (
    <Container fluid className="difference">
      <Row>
        <Col className="callstack-col" xs="12" xl="1" xxl="2">
          <Callstack
            compFunName={compare}
            oldCallStack={diff['old-callstack']}
            newCallStack={diff['new-callstack']}
            onSelect={setFunctionToShow}
            selectedFunction={functionToShow}
            definitions={definitions}
          />
        </Col>
        <Col>
          {errorCodeMessage ? (
            <Alert>
              Unable to show code:
              {errorCodeMessage}
            </Alert>
          ) : (
            codeBlock
          )}
        </Col>
      </Row>
    </Container>
  );
}

Difference.propTypes = {
  compare: PropTypes.string.isRequired,
  diff: DiffPropTypes.isRequired,
  oldFolder: PropTypes.string.isRequired,
  newFolder: PropTypes.string.isRequired,
  definitions: DefinitionsPropTypes.isRequired,
  getFile: PropTypes.func.isRequired,
};
