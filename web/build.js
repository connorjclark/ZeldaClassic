import * as esbuild from 'esbuild';
import fs from 'fs';

esbuild.build({
  entryPoints: {
    playground: "./playground/playground.js",
    "editor.worker": "monaco-editor/esm/vs/editor/editor.worker.js",
  },
  entryNames: "[name]",
  bundle: true,
  outdir: "./dist",
  loader: {
    ".ttf": "file",
  },
  sourcemap: true,
  alias: {
    'onigasm': 'vscode-oniguruma',
  },
  // minify: true,
});

fs.copyFileSync(`${import.meta.dirname}/playground/index.html`, `${import.meta.dirname}/dist/index.html`);
fs.cpSync(`${import.meta.dirname}/playground/themes`, `${import.meta.dirname}/dist/themes`, { recursive: true });
