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
        Trace("it's dangerous to go alone, take this");
    }
}
`;

const debounce = (callback, wait) => {
    let timeoutId = null;
    return (...args) => {
        window.clearTimeout(timeoutId);
        timeoutId = window.setTimeout(() => {
            callback(...args);
        }, wait);
    };
}

let moduleInitialized;
async function makeZScriptModule() {
    if (moduleInitialized) return;

    Module.noInitialRun = true;
    await ZScript(Module);
    Module.compileScript = Module.cwrap('compile_script', 'int', []);
    moduleInitialized = true;
}

async function runZscriptCompiler(code) {
    const consolePath = 'out.txt';

    await makeZScriptModule(); // TODO move
    Module.FS.writeFile('tmp.zs', code);

    const exitCode = await Module.compileScript();
    const output = new TextDecoder().decode(Module.FS.readFile(consolePath));
    const success = exitCode === 0;

    // TODO: run in worker.
    const { diagnostics, metadata } = parseZscriptOutput(output);
    return { success, diagnostics, metadata };
};

export async function main() {
    // TODO loading screen
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

    monaco.editor.defineTheme('custom-theme', theme);

    const editor = monaco.editor.create(document.querySelector('.root'), {
        value: code,
        language: 'zscript',
        theme: 'custom-theme',
        automaticLayout: true,
    });

    const languages = new Map([['zscript', 'source.zscript']]);
    await wireTmGrammars(monaco, registry, languages, editor);

    async function onContentUpdated() {
        const result = await runZscriptCompiler(editor.getModel().getValue());
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
    editor.getModel().onDidChangeContent(debounce(onContentUpdated, 500));
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
