import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    // Enable jest-like global APIs (describe, it, expect)
    globals: true,
    // Bail out and emit report on 1st failure
    bail: 1,
    // The environment where tests will run (e.g., 'node' or 'jsdom' for React/Vue)
    environment: 'node',
    // Path to setup files that run before each test file
    // setupFiles: './vitest.setup.js',
    // Pattern to find test files
    include: ['./test/**/*.{test,spec}.{js,mjs,cjs,ts,mts,cts,jsx,tsx}'],
    // Coverage configuration
    coverage: {
      provider: 'v8', // or 'istanbul'
      reporter: ['text', 'html'],
    },
    reporters: ['default', 'html'], 
    outputFile: './log/nodom_test_unit_js_results.html',
  },
});