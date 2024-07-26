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
    let instance;
    instance = await ZScript({noInitialRun: true});
    console.log(instance.FS);
    // await ZScript(instance = {
    //     preRun(zscript) {
    //         instance.FS.mkdirTree('/root-fs');
    //         instance.FS.writeFile('/root-fs/tmp.zs', code);

    //         scriptPath = '/root-fs/' + scriptPath;
    //         consolePath = '/root-fs/' + consolePath;

    //         // For some reason `set_config_file` errors if the file doesn't exist ...
    //         if (!instance.FS.analyzePath('/local/zscript.cfg').exists) instance.FS.writeFile('/local/zscript.cfg', '');

    //         zscript.instance.FS.mkdir('/root-fs');
    //         zscript.instance.FS.mount(PROXYFS, {
    //             root: '/',
    //             fs: instance.FS,
    //         }, '/root-fs');
    //         zscript.instance.FS.chdir('/root-fs');
    //     },
    //     onExit: onExitPromiseResolve,
    //     arguments: ['-linked', '-input', scriptPath, '-console', consolePath, '-qr', qr],
    // });

    // const exitCode = await onExitPromise;
    // // Not necessary, but avoids lag when playing the sfx.
    // await new Promise(resolve => setTimeout(resolve, 100));
    // console.log(exitCode);
    // return { exitCode };
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

    runZscriptCompiler('/root-fs/tmp.zs', '/root-fs/out.txt', '');
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
