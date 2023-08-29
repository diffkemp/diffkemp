// eslint-disable-next-line import/no-extraneous-dependencies
const { defineConfig } = require('cypress');

module.exports = defineConfig({
  component: {
    devServer: {
      framework: 'create-react-app',
      bundler: 'webpack',
    },
    viewportHeight: 800,
    viewportWidth: 500,
  },
});
