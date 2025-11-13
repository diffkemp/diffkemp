import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import eslint from 'vite-plugin-eslint';

export default defineConfig({
  plugins: [react(), eslint()],
  base: './',
  build: {
    outDir: 'build',
  },
  test: {
    globals: true,
    global: { vi: true },
    environment: 'jsdom',
    css: true,
  },
});
