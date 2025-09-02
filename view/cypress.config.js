// eslint-disable-next-line import/no-extraneous-dependencies
import { defineConfig } from 'cypress';
import { devServer } from '@cypress/vite-dev-server';
// eslint-disable-next-line import/extensions
import viteConfig from './vite.config.js';

export default defineConfig({
  component: {
    devServer(devServerConfig) {
      return devServer({
        ...devServerConfig,
        framework: 'react',
        bundler: 'vite',
        viteConfig,
      });
    },
    viewportHeight: 800,
    viewportWidth: 500,
  },
});
