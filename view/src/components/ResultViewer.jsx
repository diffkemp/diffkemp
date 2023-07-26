// Component for visualisation of found semantic differences.
// Author: Lukas Petr

import { useEffect, useState } from 'react';
import { PropTypes } from 'prop-types';
import { load } from 'js-yaml';
import path from 'path-browserify';

import Navbar from 'react-bootstrap/Navbar';
import Container from 'react-bootstrap/Container';
import ResultNavigation from './ResultNavigation';
import FunctionListing from './FunctionListing';
import Difference from './Difference';

import Result from '../Result';

const DEFAULT_DESCRIPTION_FILE = 'diffkemp-out.yaml';

/**
 * Component for visualisation of results (found differences) from semantic comparison.
 * @param {Object} props
 * @param {function} props.getFile - Function for getting file.
 */
export default function ResultViewer({ getFile }) {
  const [resultDescription, setResultDescription] = useState(null);
  const [comparedFunction, setComparedFunction] = useState(null);
  const [diffFunction, setDiffFunction] = useState(null);

  // Getting yaml file describing content of output directory.
  useEffect(() => {
    const getResultDescription = async () => {
      const yamlFile = await getFile(DEFAULT_DESCRIPTION_FILE);
      setResultDescription(new Result(load(yamlFile, 'utf8')));
    };
    getResultDescription();
  }, [getFile]);

  if (resultDescription === null) {
    return <>Loading...</>;
  }
  const oldSnapshotName = path.basename(resultDescription.oldSnapshot);
  const newSnapshotName = path.basename(resultDescription.newSnapshot);

  let content = null;
  if (comparedFunction && diffFunction) {
    // showing diff
    content = (
      <Difference
        key={comparedFunction + diffFunction}
        compare={comparedFunction}
        diff={resultDescription.getDiff(comparedFunction, diffFunction)}
        definitions={resultDescription.definitions}
        getFile={getFile}
        oldFolder={oldSnapshotName}
        newFolder={newSnapshotName}
      />
    );
  } else if (comparedFunction) {
    // showing selection of differing functions
    content = (
      <FunctionListing
        headline={`Differing functions for compared function '${comparedFunction}'`}
        functions={resultDescription.getDiffFuns(comparedFunction)}
        onFunctionSelect={setDiffFunction}
      />
    );
  } else if (diffFunction) {
    // showing compared functions for differing function
    content = (
      <FunctionListing
        headline={`Compared functions which differs in '${diffFunction}'`}
        functions={resultDescription.getCompFuns(diffFunction)}
        onFunctionSelect={setComparedFunction}
      />
    );
  } else {
    // showing selection of compared functions
    content = (
      <FunctionListing
        headline="Compared functions with differences found"
        functions={resultDescription.getCompFuns()}
        onFunctionSelect={setComparedFunction}
      />
    );
  }

  return (
    <>
      <Navbar bg="primary" variant="dark">
        <Container>
          <Navbar.Brand>
            Results of:
            {' '}
            {oldSnapshotName}
            {' '}
            &
            {newSnapshotName}
          </Navbar.Brand>
        </Container>
      </Navbar>
      <ResultNavigation
        comparedFunction={comparedFunction}
        diffFunction={diffFunction}
        result={resultDescription}
        setComparedFunction={setComparedFunction}
        setDiffFunction={setDiffFunction}
      />
      {content}
    </>
  );
}

ResultViewer.propTypes = {
  getFile: PropTypes.func.isRequired,
};
