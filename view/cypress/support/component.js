// This file is processed and loaded automatically before cypress test files.

// Disabling error about using dev dependency
// eslint-disable-next-line import/no-extraneous-dependencies
import { mount } from 'cypress/react';

import {} from '../../src/setup';

Cypress.Commands.add('mount', mount);
