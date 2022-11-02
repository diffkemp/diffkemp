// Listing of functions.
// Author: Lukas Petr

import Stack from 'react-bootstrap/Stack';
import Button from 'react-bootstrap/Button';
import Container from 'react-bootstrap/Container';
import Row from 'react-bootstrap/Row';
import Col from 'react-bootstrap/Col';

/**
 * Component for listing of functions.
 * @param {Object} props
 * @param {string} props.headline Name of the listing.
 * @param {Array} props.functions Array of sorted function names.
 * @param {Function} props.onFunctionSelect Callback which is called
 * on clicking function with name of clicked function.
 */
export default function FunctionListing({
  headline,
  functions,
  onFunctionSelect,
}) {
  const rows = [];
  let lastFunctionLetter = null;
  // creating buttons with function names and labels
  functions.forEach((fun) => {
    const firstChar = fun.charAt(0).toUpperCase();
    // creating label containing first letter of function
    // if the function starts with different letter than previous
    if (lastFunctionLetter !== firstChar) {
      rows.push(<b key={firstChar}>{firstChar}</b>);
      lastFunctionLetter = firstChar;
    }
    rows.push(
      <Button
        className="text-start ms-2"
        size="sm"
        onClick={() => onFunctionSelect(fun)}
        key={fun}
      >
        {fun}
      </Button>,
    );
  });
  // shows at least 20 items in column
  const stepSize = Math.max(20, Math.ceil(rows.length / 4));
  // listing functions and dividing them into multiple columns
  return (
    <Container>
      <h2>{headline}</h2>
      <Container>
        <Row xl="4" xs="1" md="2">
          <Col className="order-xl-1 order-md-1">
            <Stack gap={1}>
              {rows.slice(0 * stepSize, 1 * stepSize)}
            </Stack>
          </Col>
          <Col className="order-xl-2 order-md-3">
            <Stack gap={1}>
              {rows.slice(1 * stepSize, 2 * stepSize)}
            </Stack>
          </Col>
          <Col className="order-xl-3 order-md-2">
            <Stack gap={1}>
              {rows.slice(2 * stepSize, 3 * stepSize)}
            </Stack>
          </Col>
          <Col className="order-xl-4 order-md-4">
            <Stack gap={1}>
              {rows.slice(3 * stepSize, rows.length)}
            </Stack>
          </Col>
        </Row>
      </Container>
    </Container>
  );
}
