import * as monaco from 'monaco-editor';

import { loadWASM } from 'vscode-oniguruma'
import { Registry } from 'monaco-textmate'
import { wireTmGrammars } from 'monaco-editor-textmate'
import * as theme from './themes/solarized-dark-color-theme.json';

const code = `ffc script OldMan
{
    void run()
    {
        Trace("it's dangerous to go alone, take this");
    }
}
`;

async function runZscriptCompiler(scriptPath, consolePath, qr) {
    let onExitPromiseResolve;
    const onExitPromise = new Promise(resolve => onExitPromiseResolve = resolve);

    Module.preRun.push(() => {
        // Module.FS.mkdirTree('/root-fs');
        Module.FS.writeFile(scriptPath, code);
        // console.log(Module.FS.readdir('/include'));

        // scriptPath = scriptPath;
        // consolePath = consolePath;

        // For some reason `set_config_file` errors if the file doesn't exist ...
        // if (!Module.FS.analyzePath('/local/zscript.cfg').exists) Module.FS.writeFile('/local/zscript.cfg', '');

        // Module.FS.mkdir('/root-fs');
        // Module.FS.mount(PROXYFS, {
        //     root: '/',
        //     fs: Module.FS,
        // }, '/root-fs');
        // Module.FS.chdir('/root-fs');
    });
    // Module.postRun = () => {
    //     console.log(Module.FS.readdir('/include'));
    //     console.log(123);
    // }
    Module.arguments = ['-unlinked', '-input', scriptPath,
        // TODO
        // '-qr', qr
    ];
    try {
        await ZScript(Module);
    } finally {
        Module.preRun.unshift();
    }

    const exitCode = await onExitPromise;
    // Not necessary, but avoids lag when playing the sfx.
    await new Promise(resolve => setTimeout(resolve, 100));
    // const output = new TextDecoder().decode(module.FS.readFile(consolePath));
    // console.log(exitCode, output);

    return { exitCode };
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

    runZscriptCompiler('tmp.zs', 'out.txt', '');
    // const cb = () => {
    // };
    // if (!window.Module) {
    //     document.addEventListener('DOMContentLoaded', cb);
    // } else {
    //     cb();
    // }
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
