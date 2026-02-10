import globals from 'globals';
import pluginJs from '@eslint/js';
import sonarjs from 'eslint-plugin-sonarjs';

/** @type {import('eslint').Linter.Config[]} */
export default [
  { files: ['**/*.{js}'] },
  { languageOptions: { globals: { ...globals.browser } } },
  pluginJs.configs.recommended,
  sonarjs.configs.recommended,
  {
    rules: {
      'sonarjs/todo-tag': 'warn',
    },
  },
  { ignores: ['dist/', 'node_modules/'] },
];