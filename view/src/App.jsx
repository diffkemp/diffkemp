// Main component of application
// Author: Lukas Petr

import { useEffect, useState } from 'react';
import { load } from 'js-yaml';
import path from 'path-browserify';

import 'bootstrap/dist/css/bootstrap.min.css';
import 'react-diff-view/style/index.css';
import 'prismjs/themes/prism.min.css';
import 'prism-color-variables/variables.css';
import 'prism-color-variables/themes/visual-studio.css';
import './style.css';

import Navbar from 'react-bootstrap/Navbar';
import Container from 'react-bootstrap/Container';
import ResultNavigation from './components/ResultNavigation';
import FunctionListing from './components/FunctionListing';
import Difference from './components/Difference';

import Result from './Result';

const DEFAULT_DESCRIPTION_FILE = 'diffkemp-out.yaml';
const DEFAULT_DIRECTORY = 'output';

/**
 * Returns Promise with content of the file from DEFAULT_DIRECTORY as string.
 * @param {string} filePath Relative path to the file from DEFAULT_DIRECTORY.
 */
async function getFileWithAjax(filePath) {
  const path = [DEFAULT_DIRECTORY, filePath].join('/');
  const response = await fetch(path);
  return response.text();
}

function App() {
  const [resultDescription, setResultDescription] = useState(null);
  const [comparedFunction, setComparedFunction] = useState(null);
  const [diffFunction, setDiffFunction] = useState(null);

  // Getting yaml file describing content of output directory.
  useEffect(() => {
    const getResultDescription = async () => {
      const yamlFile = await getFileWithAjax(DEFAULT_DESCRIPTION_FILE);
      setResultDescription(new Result(load(yamlFile, 'utf8')));
    };
    getResultDescription();
  }, []);

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
        getFile={getFileWithAjax}
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
        onResultsClick={() => {
          setComparedFunction(null);
          setDiffFunction(null);
        }}
        onCompareClick={() => {
          setDiffFunction(null);
        }}
        onDifferingClick={() => {
          setComparedFunction(null);
        }}
      />
      {content}
    </>
  );
}

export default App;
