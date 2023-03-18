// Call stack visualisation.
// Author: Lukas Petr

import ListGroup from 'react-bootstrap/ListGroup';

/**
 * Component for visualisation of call stack.
 * @param {Object} props
 * @param {string} props.compFunName - First (compared) function.
 * @param {string} props.selectedFunction - Function which is selected
 * and should be highlighted in call stack.
 * @param {Function} props.onSelect - Callback function accepting
 * a function which was chosen to be shown.
 * @returns Returns call stack.
 */
export default function Callstack({
  compFunName,
  oldCallStack,
  newCallStack,
  selectedFunction,
  onSelect,
}) {
  const funListNames = oldCallStack.map((call) => call.name);
  // adding compared function
  funListNames.unshift(compFunName);

  const callstackButtons = funListNames.map((fun) => {
    const nameWrap = fun.replace(' ', '<br/>');
    return (
      <ListGroup.Item
        as="li"
        key={fun}
        title={fun}
        action
        className="callstack-call"
        onClick={() => onSelect(fun)}
        active={fun === selectedFunction}
        dangerouslySetInnerHTML={{ __html: nameWrap }}
      />
    );
  });

  return (
    <div className="callstack" data-testid="callstack">
      <div className="ms-1">
        <b>Call stack</b>
      </div>
      <ListGroup className="callstack-calls" as="ul">
        {callstackButtons}
      </ListGroup>
    </div>
  );
}
