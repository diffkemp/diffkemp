// Integration tests of Difference, Code and Callstack
// Author: Lukas Petr

import {
  render, screen, within, waitFor,
} from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import '@testing-library/jest-dom';

import Difference from '../components/Difference';

const compare = '__task_pid_nr_ns';
const diff = {
  function: 'task_struct',
  'old-callstack': [
    { name: 'task_active_pid_ns', line: 422, file: 'kernel/pid.c' },
    { name: 'task_pid', line: 440, file: 'kernel/pid.c' },
    {
      name: 'task_struct (type)',
      line: 1219,
      file: './include/linux/sched.h',
    },
  ],
  'new-callstack': [
    { name: 'task_active_pid_ns', line: 434, file: 'kernel/pid.c' },
    { name: 'task_pid', line: 445, file: 'kernel/pid.c' },
    {
      name: 'task_struct (type)',
      line: 1263,
      file: './include/linux/sched.h',
    },
  ],
};
const definitions = {
  __task_pid_nr_ns: {
    kind: 'function',
    old: {
      line: 415,
      file: 'kernel/pid.c',
      'end-line': 435,
    },
    new: {
      line: 427,
      file: 'kernel/pid.c',
      'end-line': 440,
    },
    diff: true,
  },
  task_struct: {
    kind: 'type',
    old: {
      line: 594,
      file: 'include/linux/sched.h',
      'end-line': 1217,
    },
    new: {
      line: 594,
      file: 'include/linux/sched.h',
      'end-line': 1261,
    },
    diff: true,
  },
  task_active_pid_ns: {
    kind: 'function',
    old: {
      line: 438,
      file: 'kernel/pid.c',
      'end-line': 441,
    },
    new: {
      line: 443,
      file: 'kernel/pid.c',
      'end-line': 446,
    },
    diff: false,
  },
};

const oldFolder = 'linux-4.18.0-80.el8';
const newFolder = 'linux-4.18.0-147.el8';

// mock getFile, DiffViewWrapper
const getFile = async (filePath) => (
  // returns file path as content of file
  filePath
);

const mockPropsDiffViewWrapper = jest.fn();
jest.mock('../components/DiffViewWrapper', () => function mockComponentDiffViewWrapper(props) {
  mockPropsDiffViewWrapper(props);
  return <mock-DiffViewWrapper />;
});

const setup = async () => {
  render(
    <Difference
      compare={compare}
      diff={diff}
      definitions={definitions}
      getFile={getFile}
      oldFolder={oldFolder}
      newFolder={newFolder}
    />,
  );
  // waiting for async update of state caused by getFile
  await screen.findByText(new RegExp(oldFolder));
};

test('all function names in callstack should be visible', () => {
  setup();
  const callstack = within(screen.getByTestId('callstack'));
  expect(callstack.getByText('__task_pid_nr_ns')).toBeVisible();
  expect(callstack.getByText('task_active_pid_ns')).toBeVisible();
  expect(callstack.getByText('task_pid')).toBeVisible();
  expect(callstack.getByText(/task_struct/)).toBeVisible();
});

test('first shown function should be differing function', async () => {
  setup();
  await waitFor(() => {
    expect(mockPropsDiffViewWrapper).toHaveBeenLastCalledWith(
      expect.objectContaining({
        oldCode: 'src-old/include/linux/sched.h',
        diff: 'diffs/task_struct.diff',
        oldStart: 594,
        newStart: 594,
        oldEnd: 1217,
        showDiff: true,
        linesToShow: null,
      }),
    );
  });
});

test('differing function should be active function in the callstack', () => {
  setup();
  const callstack = within(screen.getByTestId('callstack'));
  expect(callstack.getByText('__task_pid_nr_ns')).not.toHaveClass('active');
  expect(callstack.getByText('task_active_pid_ns')).not.toHaveClass('active');
  expect(callstack.getByText('task_pid')).not.toHaveClass('active');
  expect(callstack.getByText(/task_struct/)).toHaveClass('active');
});

describe('after click on function in callstack', () => {
  beforeEach(() => {
    setup();
    const callstack = within(screen.getByTestId('callstack'));
    userEvent.click(callstack.getByText('task_active_pid_ns'));
  });

  test('code of selected function should be shown', async () => {
    await waitFor(() => {
      expect(mockPropsDiffViewWrapper).toHaveBeenLastCalledWith(
        expect.objectContaining({
          oldCode: 'src-old/kernel/pid.c',
          diff: '',
          oldStart: 438,
          newStart: 443,
          oldEnd: 441,
          showDiff: false,
          linesToShow: [440, 445],
        }),
      );
    });
  });

  test('selected function should be active function in callstack', () => {
    const callstack = within(screen.getByTestId('callstack'));
    expect(callstack.getByText('__task_pid_nr_ns')).not.toHaveClass(
      'active',
    );
    expect(callstack.getByText('task_active_pid_ns')).toHaveClass('active');
    expect(callstack.getByText('task_pid')).not.toHaveClass('active');
    expect(callstack.getByText(/task_struct/)).not.toHaveClass('active');
  });

  test('correct file paths should be shown', async () => {
    expect(
      await screen.findByText('linux-4.18.0-80.el8/kernel/pid.c'),
    ).toBeVisible();
    expect(
      await screen.findByText('linux-4.18.0-147.el8/kernel/pid.c'),
    ).toBeVisible();
  });
});
