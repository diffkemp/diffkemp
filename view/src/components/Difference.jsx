// Component for visualisation of found difference
// Author: Lukas Petr

import { useState } from 'react';

import Container from 'react-bootstrap/Container';
import Col from 'react-bootstrap/Col';
import Row from 'react-bootstrap/Row';
import Alert from 'react-bootstrap/Alert';

import Callstack from './Callstack';
import Code from './Code';

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
  // name of function to be shown (defaultly showing differing function)
  const [functionToShow, setFunctionToShow] = useState(
    diff['old-callstack'].length > 0
      ? diff['old-callstack'][diff['old-callstack'].length - 1].name
      : compare,
  );
  let codeBlock = null;
  let errorCodeMessage = null;
  // extracting only name (putting away kind)
  const name = functionToShow.split(' ')[0];

  if (name in definitions) {
    const oldDefintion = definitions[name].old;
    const newDefintion = definitions[name].new;

    // returns line where is called next function in callstack
    // or undefined if it is differing function
    const getCallingLine = () => {
      // indexes to callstack containing called function
      let oldIndex = 0;
      let newIndex = 0;
      if (functionToShow !== compare) {
        oldIndex = diff['old-callstack'].findIndex(
          (call) => call.name === functionToShow,
        ) + 1;
        newIndex = diff['new-callstack'].findIndex(
          (call) => call.name === functionToShow,
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
    if (!('end-line' in oldDefintion) || !('end-line' in newDefintion)) {
      errorCodeMessage = 'Missing info about ending of function.';
    } else {
      // creating component for code visualisation
      // if all information are included for showing code of function
      const specification = {
        oldSrc: oldDefintion.file,
        newSrc: newDefintion.file,
        diff: definitions[name].diff
          ? `${name}.diff`
          : null,
        oldStart: oldDefintion.line,
        newStart: newDefintion.line,
        oldEnd: oldDefintion['end-line'],
        calling: getCallingLine(),
      };
      codeBlock = (
        <Code
          key={`${compare}-${functionToShow}`}
          specification={specification}
          getFile={getFile}
          oldFolder={oldFolder}
          newFolder={newFolder}
        />
      );
    }
  } /* if (name in definitions) */ else {
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
