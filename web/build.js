import * as esbuild from 'esbuild';

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
  minify: false,
});
