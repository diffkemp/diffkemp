// Integration tests of ResultNavigation, NavigationArrows, Result
// Author: Lukas Petr

import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import '@testing-library/jest-dom';

import Result from '../Result';
import ResultNavigation from '../components/ResultNavigation';

const resultYAML = {
  'old-snapshot': '/tests/SNAPSHOT/kernel/linux-4.18.0-80.el8/',
  'new-snapshot': '/tests/SNAPSHOT/kernel/linux-4.18.0-147.el8/',
  results: [
    {
      function: 'down_read',
      diffs: [
        {
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
        {
          function: 'down_read',
          'old-callstack': [],
          'new-callstack': [],
        },
      ],
    },
    {
      function: 'scsi_host_alloc',
      diffs: [
        {
          function: 'scsi_host_alloc',
          'old-callstack': [],
          'new-callstack': [],
        },
      ],
    },
  ],
};

const result = new Result(resultYAML);

const handleCompareFun = jest.fn();
const handleDiffFun = jest.fn();

afterEach(() => {
  handleCompareFun.mockClear();
  handleDiffFun.mockClear();
});

describe('showing first differing function of first compared function', () => {
  const setup = () => {
    render(
      <ResultNavigation
        comparedFunction="down_read"
        diffFunction="__down_read"
        setComparedFunction={handleCompareFun}
        setDiffFunction={handleDiffFun}
        result={result}
      />,
    );
  };

  test('navigation should show correct name of compared and differing function', () => {
    setup();
    expect(screen.getByTitle(/compared function/i)).toHaveTextContent(
      'down_read',
    );
    expect(screen.getByTitle(/differing function/i)).toHaveTextContent(
      '__down_read',
    );
  });

  test('next and prev buttons should be visible', () => {
    setup();
    expect(screen.getByText(/next compared/i)).toBeVisible();
    expect(screen.getByText(/prev compared/i)).toBeVisible();
    expect(screen.getByText(/next differing/i)).toBeVisible();
    expect(screen.getByText(/prev differing/i)).toBeVisible();
  });

  test('click on Result should reset both functions', () => {
    setup();
    userEvent.click(screen.getByText(/results/i));
    expect(handleDiffFun).toHaveBeenLastCalledWith(null);
    expect(handleCompareFun).toHaveBeenLastCalledWith(null);
  });

  test('click on compare fun should reset differing function', () => {
    setup();
    userEvent.click(screen.getByTitle(/compared function/i));
    expect(handleDiffFun).toHaveBeenLastCalledWith(null);
    expect(handleCompareFun).toHaveBeenCalledTimes(0);
  });

  test('click on differing fun should reset compared function', () => {
    setup();
    userEvent.click(screen.getByTitle(/differing function/i));
    expect(handleCompareFun).toHaveBeenLastCalledWith(null);
    expect(handleDiffFun).toHaveBeenCalledTimes(0);
  });

  test('click on next differing should correctly set differing function', () => {
    setup();
    userEvent.click(screen.getByText(/next differing/i));
    expect(handleDiffFun).toHaveBeenLastCalledWith('down_read');
    expect(handleCompareFun).toHaveBeenCalledTimes(0);
  });

  test('click on prev differing should be disabled', () => {
    setup();
    expect(screen.getByText(/prev differing/i)).toBeDisabled();
  });

  test('click on prev compared should be disabled', () => {
    setup();
    expect(screen.getByText(/prev compared/i)).toBeDisabled();
  });

  test('click on next compared should correctly set functions', () => {
    setup();
    userEvent.click(screen.getByText(/next compared/i));
    expect(handleCompareFun).toHaveBeenLastCalledWith('scsi_host_alloc');
    expect(handleDiffFun).toHaveBeenLastCalledWith('scsi_host_alloc');
  });
});

describe('showing last differing function of first compared function', () => {
  const setup = () => {
    render(
      <ResultNavigation
        comparedFunction="down_read"
        diffFunction="down_read"
        setComparedFunction={handleCompareFun}
        setDiffFunction={handleDiffFun}
        result={result}
      />,
    );
  };

  test('click on prev differing should correctly set differing function', () => {
    setup();
    userEvent.click(screen.getByText(/prev differing/i));
    expect(handleDiffFun).toHaveBeenLastCalledWith('__down_read');
    expect(handleCompareFun).toHaveBeenCalledTimes(0);
  });

  test('click on next differing should be disabled', () => {
    setup();
    expect(screen.getByText(/next differing/i)).toBeDisabled();
  });
});

describe('showing only differing function of last compared function', () => {
  const setup = () => {
    render(
      <ResultNavigation
        comparedFunction="scsi_host_alloc"
        diffFunction="scsi_host_alloc"
        setComparedFunction={handleCompareFun}
        setDiffFunction={handleDiffFun}
        result={result}
      />,
    );
  };

  test('click on prev compared should correctly set differing function', () => {
    setup();
    userEvent.click(screen.getByText(/prev compared/i));
    expect(handleDiffFun).toHaveBeenLastCalledWith('__down_read');
    expect(handleCompareFun).toHaveBeenLastCalledWith('down_read');
  });

  test('click on next compared should be disabled', () => {
    setup();
    expect(screen.getByText(/next compared/i)).toBeDisabled();
  });

  test('click on next differing should be disabled', () => {
    setup();
    expect(screen.getByText(/next differing/i)).toBeDisabled();
  });

  test('click on prev differing should be disabled', () => {
    setup();
    expect(screen.getByText(/prev differing/i)).toBeDisabled();
  });
});

describe('showing differing functions for first compared function', () => {
  const setup = () => {
    render(
      <ResultNavigation
        comparedFunction="down_read"
        diffFunction={null}
        setComparedFunction={handleCompareFun}
        setDiffFunction={handleDiffFun}
        result={result}
      />,
    );
  };

  test('navigation should show correct name of compared function', () => {
    setup();
    expect(screen.getByTitle(/compared function/i)).toHaveTextContent(
      'down_read',
    );
  });

  test('next and prev buttons should be visible', () => {
    setup();
    expect(screen.getByText(/next compared/i)).toBeVisible();
    expect(screen.getByText(/prev compared/i)).toBeVisible();
  });

  test('click on next compared should correctly set differing function', () => {
    setup();
    userEvent.click(screen.getByText(/next compared/i));
    expect(handleCompareFun).toHaveBeenLastCalledWith('scsi_host_alloc');
  });

  test('click on prev compared should be disabled', () => {
    setup();
    expect(screen.getByText(/prev compared/i)).toBeDisabled();
  });
});

describe('showing differing functions for last compared function', () => {
  const setup = () => {
    render(
      <ResultNavigation
        comparedFunction="scsi_host_alloc"
        diffFunction={null}
        setComparedFunction={handleCompareFun}
        setDiffFunction={handleDiffFun}
        result={result}
      />,
    );
  };

  test('click on prev compared should correctly set differing function', () => {
    setup();
    userEvent.click(screen.getByText(/prev compared/i));
    expect(handleCompareFun).toHaveBeenLastCalledWith('down_read');
  });

  test('click on next compared should be disabled', () => {
    setup();
    expect(screen.getByText(/next compared/i)).toBeDisabled();
  });
});
