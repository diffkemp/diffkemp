// Main component of application
// Author: Lukas Petr

import 'bootstrap/dist/css/bootstrap.min.css';
import 'react-diff-view/style/index.css';
import 'prismjs/themes/prism.min.css';
import 'prism-color-variables/variables.css';
import 'prism-color-variables/themes/visual-studio.css';
import './style.css';

import ResultViewer from './components/ResultViewer';

/**
 * Returns Promise with content of the file as string.
 * @param {string} filePath Path to the file.
 */
async function getFileWithAjax(filePath) {
  const response = await fetch(filePath);
  return response.text();
}

function App() {
  return <ResultViewer getFile={getFileWithAjax} />;
}

export default App;
