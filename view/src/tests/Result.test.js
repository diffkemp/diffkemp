// Tests of Result class
// Author: Lukas Petr
import { beforeAll } from 'vitest';

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

const yamlGroups = {
  results: [
    {
      function: 'fun3',
      diffs: [
        {
          function: 'fun2',
          'old-callstack': [],
          'new-callstack': [],
        },
        {
          function: 'fun3',
          'old-callstack': [],
          'new-callstack': [],
        },
      ],
    },
    {
      glob_var: 'glob1',
      results: [
        {
          function: 'fun1',
          diffs: [
            {
              function: 'fun2',
              'old-callstack': [],
              'new-callstack': [],
            },
            {
              function: 'fun1',
              'old-callstack': [],
              'new-callstack': [],
            },
          ],
        },
      ],
    },
    {
      multi_group: 'multi1',
      results: [
        {
          glob_var: 'glob2',
          results: [
            {
              function: 'fun4',
              diffs: [
                {
                  function: 'fun4',
                  'old-callstack': [],
                  'new-callstack': [],
                },
              ],
            },
          ],
        },
      ],
    },
  ],
};

let result = null;
let resultGroups = null;

beforeAll(() => {
  result = new Result(yaml);
  resultGroups = new Result(yamlGroups);
});

test('getCompFuns should return sorted array of all compared functions', () => {
  expect(result.getCompFuns()).toEqual(['down_read', 'scsi_host_alloc']);
  expect(resultGroups.getCompFuns()).toEqual(['fun1', 'fun3', 'fun4']);
});

test('getCompFuns for differing function should return sorted array of compared functions which differs in specified function', () => {
  expect(result.getCompFuns('__down_read')).toEqual([
    'down_read',
    'scsi_host_alloc',
  ]);
  expect(resultGroups.getCompFuns('fun2')).toEqual([
    'fun1',
    'fun3',
  ]);
});

test('getDiffFuns should return sorted array of all differing functions', () => {
  expect(result.getDiffFuns()).toEqual([
    '__down_read',
    'down_read',
    'scsi_host_alloc',
  ]);
  expect(resultGroups.getDiffFuns()).toEqual([
    'fun1',
    'fun2',
    'fun3',
    'fun4',
  ]);
});

test('getDiffFuns for compared function should return sorted array of function in which the compared function differs', () => {
  expect(result.getDiffFuns('scsi_host_alloc')).toEqual([
    '__down_read',
    'scsi_host_alloc',
  ]);
  expect(resultGroups.getDiffFuns('fun1')).toEqual([
    'fun1',
    'fun2',
  ]);
});

test('getNextCompName should return next compared function', () => {
  expect(result.getNextCompName('down_read')).toEqual('scsi_host_alloc');
  expect(resultGroups.getNextCompName('fun1')).toEqual('fun3');
});

test('getPrevCompName should return previous compared function', () => {
  expect(result.getPrevCompName('scsi_host_alloc')).toEqual('down_read');
  expect(resultGroups.getPrevCompName('fun4')).toEqual('fun3');
});

test('getNextDiffFunNameForComp should return next differing function for compared function', () => {
  expect(
    result.getNextDiffFunNameForComp('down_read', '__down_read'),
  ).toEqual('down_read');
  expect(
    resultGroups.getNextDiffFunNameForComp('fun3', 'fun2'),
  ).toEqual('fun3');
});

test('getPrevDiffFunNameForComp should return previous differing function for compared function', () => {
  expect(result.getPrevDiffFunNameForComp('down_read', 'down_read')).toEqual(
    '__down_read',
  );
  expect(resultGroups.getPrevDiffFunNameForComp('fun1', 'fun2')).toEqual(
    'fun1',
  );
});

test('getDiff should return item from results diffs for specified compared and differing function', () => {
  expect(result.getDiff('down_read', 'down_read')).toBe(
    yaml.results[0].diffs[1],
  );
  expect(resultGroups.getDiff('fun4', 'fun4')).toBe(
    yamlGroups.results[2].results[0].results[0].diffs[0],
  );
});

test('isFirstCompFun should return true for first compared function', () => {
  expect(result.isFirstCompFun('down_read')).toBe(true);
  expect(resultGroups.isFirstCompFun('fun1')).toBe(true);
});
test('isLastCompFun should return true for last compared function', () => {
  expect(result.isLastCompFun('scsi_host_alloc')).toBe(true);
  expect(resultGroups.isLastCompFun('fun4')).toBe(true);
});
test('isFirstDiffFunForComp should return true for first differing function of compared function', () => {
  expect(result.isFirstDiffFunForComp('down_read', '__down_read')).toBe(true);
  expect(resultGroups.isFirstDiffFunForComp('fun1', 'fun1')).toBe(true);
});
test('isLastDiffFunForComp should return true for last differing function of compared function', () => {
  expect(result.isLastDiffFunForComp('down_read', 'down_read')).toBe(true);
  expect(resultGroups.isLastDiffFunForComp('fun4', 'fun4')).toBe(true);
});
test('getFirstDiffFunForComp should return first differing function for compared function', () => {
  expect(result.getFirstDiffFunForComp('down_read')).toEqual('__down_read');
  expect(resultGroups.getFirstDiffFunForComp('fun3')).toEqual('fun2');
});
