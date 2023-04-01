// Integration tests of ResultViewer, FunctionListing, ResultNavigation
// with mocked Difference component
// Author: Lukas Petr

import { render, screen } from '@testing-library/react';
import fireEvent from '@testing-library/user-event';
import '@testing-library/jest-dom';

import ResultViewer from '../components/ResultViewer';

const result = `old-snapshot: /tests/SNAPSHOT/kernel/linux-4.18.0-80.el8/
new-snapshot: /tests/SNAPSHOT/kernel/linux-4.18.0-147.el8/
results:
- function: down_read
  diffs:
  - function: __down_read
    old-callstack: 
    - name: __down_read
      line: 26
      file: kernel/locking/rwsem.c
    new-callstack:
    - name: __down_read
      line: 26
      file: kernel/locking/rwsem.c
  - function: down_read
    old-callstack: []
    new-callstack: []
- function: scsi_host_alloc
  diffs:
  - function: scsi_host_alloc
    old-callstack: []
    new-callstack: []
definitions: {}
`;

const mockPropsDifference = jest.fn();
jest.mock('../components/Difference', () => function mockComponentDifference(props) {
  mockPropsDifference(props);
  return <mock-Difference />;
});

/**
 * Mocked getFile to return result when trying to get file
 * describing result, else return file path of file which is trying to get.
 */
const getFile = async (filePath) => {
  if (filePath === 'diffkemp-out.yaml') {
    return result;
  }
  return filePath;
};

const setup = () => {
  render(<ResultViewer getFile={getFile} />);
};

test('name of compared snapshot folders should be visible', async () => {
  setup();
  expect(await screen.findByText(/linux-4.18.0-80.el8/)).toBeVisible();
  expect(await screen.findByText(/linux-4.18.0-147.el8/)).toBeVisible();
});

test('after selecting compared and differing function correct props should be passed to Difference component', async () => {
  setup();
  const compareFunBtn = await screen.findByRole('button', {
    name: 'down_read',
  });
  fireEvent.click(compareFunBtn);
  const diffFunBtn = await screen.findByRole('button', {
    name: '__down_read',
  });
  fireEvent.click(diffFunBtn);
  expect(mockPropsDifference).toHaveBeenLastCalledWith({
    compare: 'down_read',
    diff: {
      function: '__down_read',
      'old-callstack': [
        {
          name: '__down_read',
          line: 26,
          file: 'kernel/locking/rwsem.c',
        },
      ],
      'new-callstack': [
        {
          name: '__down_read',
          line: 26,
          file: 'kernel/locking/rwsem.c',
        },
      ],
    },
    definitions: {},
    getFile,
    oldFolder: 'linux-4.18.0-80.el8',
    newFolder: 'linux-4.18.0-147.el8',
  });
});

test('user should be able to return to compared function listing from visualisation of concrete difference', async () => {
  setup();
  const compareFunBtn = await screen.findByRole('button', {
    name: 'down_read',
  });
  fireEvent.click(compareFunBtn);
  const diffFunBtn = await screen.findByRole('button', {
    name: '__down_read',
  });
  fireEvent.click(diffFunBtn);
  // trying to return to compared function listing
  fireEvent.click(await screen.findByRole('button', { name: /results/i }));
  expect(
    screen.getByText('Compared functions with differences found'),
  ).toBeVisible();
});

test('clicking on next differing button should pass correct props to Difference component', async () => {
  setup();
  const compareFunBtn = await screen.findByRole('button', {
    name: 'down_read',
  });
  fireEvent.click(compareFunBtn);
  const diffFunBtn = await screen.findByRole('button', {
    name: '__down_read',
  });
  fireEvent.click(diffFunBtn);
  fireEvent.click(
    await screen.findByRole('button', { name: /next differing/i }),
  );
  expect(mockPropsDifference).toHaveBeenLastCalledWith({
    compare: 'down_read',
    diff: {
      function: 'down_read',
      'old-callstack': [],
      'new-callstack': [],
    },
    definitions: {},
    getFile,
    oldFolder: 'linux-4.18.0-80.el8',
    newFolder: 'linux-4.18.0-147.el8',
  });
});
