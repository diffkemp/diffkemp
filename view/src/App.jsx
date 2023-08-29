// Main component of application
// Author: Lukas Petr

// Components hierarchy
// - `App`
//   - `ResultViewer`: Visualisation of found semantic differences.
//     - `ResultNavigation`: Navigation between results (differences).
//       - `NavigationArrows`: Arrows for navigating in results.
//     - `FunctionListing`: Listing of compared/differing functions.
//     - `Difference`: Visualisation of selected difference
//                     (defined by compared and differing function).
//       - `Callstack`: Visualisation of call stack for the selected difference.
//       - `Code`: Preparation of code (fetching necessary files)
//                 for selected function from call stack.
//         - `DiffViewWrapper`: The visualisation of the selected function
//                              made possible by react-diff-view package.

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
