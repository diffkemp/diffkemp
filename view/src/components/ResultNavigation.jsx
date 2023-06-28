// Visualisation of location in results for user navigation
// Author: Lukas Petr

import { useCallback } from 'react';
import Breadcrumb from 'react-bootstrap/Breadcrumb';
import Container from 'react-bootstrap/Container';

/**
 * @param {Object} props
 * @param {string} props.comparedFunction Name of a compared function.
 * @param {string} props.diffFunction Name of a differing function.
 * @param {Function} props.setComparedFunction Callback function for seting new compared function.
 * @param {Function} props.setDiffFunction Callback function for seting new differing function.
 * @returns
 */
export default function ResultNavigation({
  comparedFunction,
  diffFunction,
  setComparedFunction,
  setDiffFunction,
}) {
  const handleResultsClick = useCallback(() => {
    setComparedFunction(null);
    setDiffFunction(null);
  }, [setComparedFunction, setDiffFunction]);

  const handleCompareClick = useCallback(() => {
    setDiffFunction(null);
  }, [setDiffFunction]);

  const handleDifferingClick = useCallback(() => {
    setComparedFunction(null);
  }, [setComparedFunction]);

  let compItem = null;
  // location in compared function
  if (comparedFunction || diffFunction) {
    compItem = (
      <Breadcrumb.Item
        onClick={handleCompareClick}
        title="Compared function"
      >
        {comparedFunction || '*'}
      </Breadcrumb.Item>
    );
  }

  return (
    <Container>
      <Breadcrumb className="result-nav d-inline-block">
        <Breadcrumb.Item onClick={handleResultsClick}>
          Results
        </Breadcrumb.Item>
        {compItem}
        {diffFunction && (
        <Breadcrumb.Item
          onClick={handleDifferingClick}
          title="Differing function"
        >
          {diffFunction}
        </Breadcrumb.Item>
        )}
      </Breadcrumb>
    </Container>
  );
}
