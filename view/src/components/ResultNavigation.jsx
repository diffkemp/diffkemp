// Visualisation of location in results for user navigation
// Author: Lukas Petr
import Breadcrumb from 'react-bootstrap/Breadcrumb';
import Container from 'react-bootstrap/Container';

/**
 * @param {Object} props
 * @param {string} props.comparedFunction Name of a compared function.
 * @param {string} props.diffFunction Name of a differing function.
 * @param {Function} props.onResultsClick Function which is called
 * when results is clicked.
 * @param {Function} props.onCompareClick Function which is called
 * when compared function is clicked.
 * @returns
 */
export default function ResultNavigation({
  comparedFunction,
  diffFunction,
  onResultsClick,
  onCompareClick,
  onDifferingClick,
}) {
  let compItem = null;
  // location in compared function
  if (comparedFunction || diffFunction) {
    compItem = (
      <Breadcrumb.Item onClick={onCompareClick} title="Compared function">
        {comparedFunction || '*'}
      </Breadcrumb.Item>
    );
  }

  return (
    <Container>
      <Breadcrumb className="result-nav d-inline-block">
        <Breadcrumb.Item onClick={onResultsClick}>
          Results
        </Breadcrumb.Item>
        {compItem}
        {diffFunction && (
        <Breadcrumb.Item
          onClick={onDifferingClick}
          title="Differing function"
        >
          {diffFunction}
        </Breadcrumb.Item>
        )}
      </Breadcrumb>
    </Container>
  );
}
