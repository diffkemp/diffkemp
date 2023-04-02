// Tests of Result class
// Author: Lukas Petr

import Result from '../Result';

const yaml = {
  results: [
    {
      function: 'down_read',
      diffs: [
        {
          function: '__down_read',
          'old-callstack': [],
          'new-callstack': [],
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
          function: '__down_read',
          'old-callstack': [],
          'new-callstack': [],
        },
        {
          function: 'scsi_host_alloc',
          'old-callstack': [],
          'new-callstack': [],
        },
      ],
    },
  ],
};

let result = null;

beforeAll(() => {
  result = new Result(yaml);
});

test('getCompFuns should return sorted array of all compared functions', () => {
  expect(result.getCompFuns()).toEqual(['down_read', 'scsi_host_alloc']);
});

test('getCompFuns for differing function should return sorted array of compared functions which differs in specified function', () => {
  expect(result.getCompFuns('__down_read')).toEqual([
    'down_read',
    'scsi_host_alloc',
  ]);
});

test('getDiffFuns should return sorted array of all differing functions', () => {
  expect(result.getDiffFuns()).toEqual([
    '__down_read',
    'down_read',
    'scsi_host_alloc',
  ]);
});

test('getDiffFuns for compared function should return sorted array of function in which the compared function differs', () => {
  expect(result.getDiffFuns('scsi_host_alloc')).toEqual([
    '__down_read',
    'scsi_host_alloc',
  ]);
});

test('getNextCompName should return next compared function', () => {
  expect(result.getNextCompName('down_read')).toEqual('scsi_host_alloc');
});

test('getPrevCompName should return previous compared function', () => {
  expect(result.getPrevCompName('scsi_host_alloc')).toEqual('down_read');
});

test('getNextDiffFunNameForComp should return next differing function for compared function', () => {
  expect(
    result.getNextDiffFunNameForComp('down_read', '__down_read'),
  ).toEqual('down_read');
});
test('getPrevDiffFunNameForComp should return previous differing function for compared function', () => {
  expect(result.getPrevDiffFunNameForComp('down_read', 'down_read')).toEqual(
    '__down_read',
  );
});
test('getDiff should return item from results diffs for specified compared and differing function', () => {
  expect(result.getDiff('down_read', 'down_read')).toBe(
    yaml.results[0].diffs[1],
  );
});

test('isFirstCompFun should return true for first compared function', () => {
  expect(result.isFirstCompFun('down_read')).toBe(true);
});
test('isLastCompFun should return true for last compared function', () => {
  expect(result.isLastCompFun('scsi_host_alloc')).toBe(true);
});
test('isFirstDiffFunForComp should return true for first differing function of compared function', () => {
  expect(result.isFirstDiffFunForComp('down_read', '__down_read')).toBe(true);
});
test('isLastDiffFunForComp should return true for last differing function of compared function', () => {
  expect(result.isLastDiffFunForComp('down_read', 'down_read')).toBe(true);
});
test('getFirstDiffFunForComp should return first differing function for compared function', () => {
  expect(result.getFirstDiffFunForComp('down_read')).toEqual('__down_read');
});
