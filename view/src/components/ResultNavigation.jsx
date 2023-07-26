// Visualisation of location in results for user navigation
// Author: Lukas Petr

import { useCallback } from 'react';
import { PropTypes } from 'prop-types';

import Breadcrumb from 'react-bootstrap/Breadcrumb';
import Container from 'react-bootstrap/Container';
import NavigationArrows from './NavigationArrows';
import Result from '../Result';

/**
 * @param {Object} props
 * @param {string} props.comparedFunction Name of a compared function.
 * @param {string} props.diffFunction Name of a differing function.
 * @param {Function} props.setComparedFunction Callback function for seting new compared function.
 * @param {Function} props.setDiffFunction Callback function for seting new differing function.
 * @param {Result} props.result Result of the comparison.
 * @returns
 */
export default function ResultNavigation({
  comparedFunction,
  diffFunction,
  setComparedFunction,
  setDiffFunction,
  result,
}) {
  const handleNextCompClick = useCallback(() => {
    const nextCompName = result.getNextCompName(comparedFunction);
    setComparedFunction(nextCompName);
    if (diffFunction) {
      setDiffFunction(result.getFirstDiffFunForComp(nextCompName));
    }
  }, [setComparedFunction, setDiffFunction, result, comparedFunction, diffFunction]);

  const handlePrevCompClick = useCallback(() => {
    const prevCompName = result.getPrevCompName(comparedFunction);
    setComparedFunction(prevCompName);
    if (diffFunction) {
      setDiffFunction(result.getFirstDiffFunForComp(prevCompName));
    }
  }, [setComparedFunction, setDiffFunction, result, comparedFunction, diffFunction]);

  const handleNextDiffClick = useCallback(() => {
    setDiffFunction(
      result.getNextDiffFunNameForComp(comparedFunction, diffFunction),
    );
  }, [setDiffFunction, result, comparedFunction, diffFunction]);

  const handlePrevDiffClick = useCallback(() => {
    setDiffFunction(
      result.getPrevDiffFunNameForComp(comparedFunction, diffFunction),
    );
  }, [setDiffFunction, result, diffFunction, comparedFunction]);

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
      {/* location in results of found differences */}
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
      <div className="float-end">
        {/* when showing differing functions (compared function is selected) */}
        {/* add arrows to move to next/prev compared function */}
        {comparedFunction && (
        <NavigationArrows
          onPrevClick={handlePrevCompClick}
          onNextClick={handleNextCompClick}
          disableNext={result.isLastCompFun(comparedFunction)}
          disablePrev={result.isFirstCompFun(comparedFunction)}
          prevText="compared"
          nextText="compared"
        />
        )}
        {/* when showing found difference (compared and differing function is selected) */}
        {/* add arrows to move to next/prev difference for compared function */}
        {comparedFunction && diffFunction && (
        <NavigationArrows
          className="ps-4"
          onPrevClick={handlePrevDiffClick}
          onNextClick={handleNextDiffClick}
          disableNext={result.isLastDiffFunForComp(
            comparedFunction,
            diffFunction,
          )}
          disablePrev={result.isFirstDiffFunForComp(
            comparedFunction,
            diffFunction,
          )}
          prevText="differing"
          nextText="differing"
        />
        )}
      </div>
    </Container>
  );
}

ResultNavigation.propTypes = {
  comparedFunction: PropTypes.string,
  diffFunction: PropTypes.string,
  setComparedFunction: PropTypes.func.isRequired,
  setDiffFunction: PropTypes.func.isRequired,
  result: PropTypes.instanceOf(Result).isRequired,
};
