// Unit tests of FunctionListing component
// Author: Lukas Petr

import { screen, render } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import '@testing-library/jest-dom';

import FunctionListing from '../components/FunctionListing';

test('headline should be visible', () => {
  render(
    <FunctionListing
      headline="Function"
      functions={[]}
      onFunctionSelect={() => {

      }}
    />,
  );
  expect(screen.getByText('Function')).toBeVisible();
});

test('function names should be visible', () => {
  const names = ['free', 'malloc', 'puts', 'printf'];
  render(
    <FunctionListing
      headline="Function"
      functions={names}
      onFunctionSelect={() => {

      }}
    />,
  );
  expect(screen.getByText('malloc')).toBeVisible();
  expect(screen.getByText('free')).toBeVisible();
  expect(screen.getByText('puts')).toBeVisible();
  expect(screen.getByText('printf')).toBeVisible();
});

test('click on function should return its name', () => {
  const names = ['free', 'malloc', 'printf'];
  const handleSelect = vi.fn();
  render(
    <FunctionListing
      headline="Function"
      functions={names}
      onFunctionSelect={handleSelect}
    />,
  );
  userEvent.click(screen.getByText('malloc'));
  expect(handleSelect).toHaveBeenCalledWith('malloc');
});
