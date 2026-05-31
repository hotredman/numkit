// ESLint flat config — guardrail focused on the bug classes that have
// actually bitten this IDE: references to undefined / moved identifiers
// (no-undef — the setActiveCell crash), dead bindings (no-unused-vars),
// and broken React hook usage. Style is intentionally NOT enforced — this
// is a correctness net, not a formatter.

import js from '@eslint/js';
import globals from 'globals';
import reactHooks from 'eslint-plugin-react-hooks';
import react from 'eslint-plugin-react';

export default [
  { ignores: ['dist/**', 'desktop/**', 'public/**', 'node_modules/**', '*.config.js'] },
  js.configs.recommended,
  {
    files: ['src/**/*.{js,jsx}'],
    languageOptions: {
      ecmaVersion: 2022,
      sourceType: 'module',
      parserOptions: { ecmaFeatures: { jsx: true } },
      globals: { ...globals.browser, ...globals.node },
    },
    plugins: { 'react-hooks': reactHooks, react },
    rules: {
      // The correctness core — these are ERRORS.
      'no-undef': 'error',
      'react-hooks/rules-of-hooks': 'error',
      // Mark JSX-referenced identifiers (<Foo/>) as used so no-unused-vars
      // doesn't false-flag component imports.
      'react/jsx-uses-vars': 'error',
      'react/jsx-uses-react': 'off',   // new JSX transform — no React import needed
      // Noisy-but-useful — WARN so they surface without blocking CI.
      // caughtErrors:'none' — an unused `catch (e)` binding is idiomatic
      // (we just want the catch), not worth flagging.
      'no-unused-vars': ['warn', {
        argsIgnorePattern: '^_', varsIgnorePattern: '^_', caughtErrors: 'none',
      }],
      'react-hooks/exhaustive-deps': 'warn',
      // JSX uses these implicitly; recommended flags them otherwise.
      'no-empty': ['warn', { allowEmptyCatch: true }],
      // Allow intentional control-flow patterns common in this codebase.
      'no-cond-assign': ['error', 'except-parens'],
    },
  },
  {
    // Test files get vitest globals.
    files: ['src/**/*.test.{js,jsx}'],
    languageOptions: { globals: { ...globals.node } },
  },
];
