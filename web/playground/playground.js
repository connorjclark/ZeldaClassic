import * as monaco from 'monaco-editor';

import { loadWASM } from 'vscode-oniguruma'
import { Registry } from 'monaco-textmate'
import { wireTmGrammars } from 'monaco-editor-textmate'
import * as theme from './themes/solarized-dark-color-theme.json';
import { parseZscriptOutput } from './parse_zscript_output';

const code = `ffc script OldMan
{
    void run()
    {
        Tracelol("it's dangerous to go alone, take this");
    }
}
`;

async function runZscriptCompiler(scriptPath) {
    const consolePath = 'output.txt';

    let onExitPromiseResolve;
    const onExitPromise = new Promise(resolve => onExitPromiseResolve = resolve);
    Module.onExit = onExitPromiseResolve;
    Module.preRun.push(() => {
        Module.FS.writeFile(scriptPath, code);
    });
    Module.arguments = [
        '-unlinked',
        '-input', scriptPath,
        '-console', consolePath,
        // TODO
        // '-qr', qr
    ];

    try {
        await ZScript(Module);
    } finally {
        Module.preRun.unshift();
    }

    const exitCode = await onExitPromise;
    const output = new TextDecoder().decode(Module.FS.readFile(consolePath));
    const success = exitCode === 0;
    const { diagnostics, metadata } = parseZscriptOutput(output);
    return { success, diagnostics, metadata };
};

export async function main() {
    await loadWASM({
        data: await fetch('onig.wasm').then(r => r.arrayBuffer())
    });

    const registry = new Registry({
        getGrammarDefinition: async () => {
            return {
                format: 'json',
                content: await fetch(`zscript.tmLanguage.json`).then(r => r.text()),
            }
        }
    });

    // monaco's built-in themes aren't powereful enough to handle TM tokens
    // https://github.com/Nishkalkashyap/monaco-vscode-textmate-theme-converter#monaco-vscode-textmate-theme-converter
    monaco.editor.defineTheme('dark-vs', theme);

    const editor = monaco.editor.create(document.querySelector('.root'), {
        value: code,
        language: 'zscript',
        theme: 'dark-vs',
        automaticLayout: true,
    });

    const languages = new Map([['zscript', 'source.zscript']]);
    await wireTmGrammars(monaco, registry, languages, editor);

    const result = await runZscriptCompiler('tmp.zs');
    const markers = result.diagnostics.map(d => {
        return {
            severity: monaco.MarkerSeverity.Error,
            message: d.message,
            startColumn: d.range.start.character + 1,
            startLineNumber: d.range.start.line + 1,
            endColumn: d.range.end.character + 1,
            endLineNumber: d.range.end.line + 1,
        };
    });
    monaco.editor.setModelMarkers(editor.getModel(), '', markers);
}

self.MonacoEnvironment = {
    getWorkerUrl: function (moduleId, label) {
        return "./editor.worker.js";
    },
};

monaco.languages.register({
    id: 'zscript',
});

main();
