import * as fs from 'fs';
import * as path from 'path';
import { ExtensionContext } from 'vscode';
import * as vscode from 'vscode';
import * as childProcess from 'child_process';
import {promisify} from 'util';

const execFile = promisify(childProcess.execFile);

/**
 * The doc comments in the engine's binding headers have their own formatter.
 * Returns its path if the document is a binding header inside a ZQuest Classic
 * checkout, otherwise undefined.
 */
function findBindingsFormatter(document: vscode.TextDocument): string | undefined {
	if (document.uri.scheme !== 'file' || !document.uri.fsPath.endsWith('.zh'))
		return;

	const bindingsDir = path.join('resources', 'include', 'bindings');
	const dir = path.dirname(document.uri.fsPath);
	if (!dir.endsWith(bindingsDir))
		return;

	const rootDir = dir.slice(0, -bindingsDir.length);
	const formatterPath = path.join(rootDir, 'scripts', 'zscript_formatter.py');
	return fs.existsSync(formatterPath) ? formatterPath : undefined;
}

async function formatBindingsDocument(document: vscode.TextDocument, formatterPath: string): Promise<vscode.TextEdit[]> {
	try {
		const text = document.getText();
		const python = process.platform === 'win32' ? 'python' : 'python3';
		const output = await new Promise<string>((resolve, reject) => {
			const cp = childProcess.execFile(python, [formatterPath, '--stdin'], {maxBuffer: 100 * 1024 * 1024}, (err, stdout) => {
				if (err)
					return reject(err);

				resolve(stdout);
			});
			cp.stdin.write(text);
			cp.stdin.end();
		});
		if (output && text !== output) {
			const r = new vscode.Range(document.lineAt(0).range.start, document.lineAt(document.lineCount - 1).range.end);
			return [vscode.TextEdit.replace(r, output)];
		}
	} catch (e) {
		vscode.window.showErrorMessage(`Error while attempting to format bindings:\n${e.message}`);
		console.log(e);
	}
	return [];
}

import {
	LanguageClient,
	LanguageClientOptions,
	ServerOptions,
	TransportKind
} from 'vscode-languageclient/node';

let client: LanguageClient;

export function activate(context: ExtensionContext) {
	// Keep the engine's binding headers formatted, without requiring any
	// configuration.
	context.subscriptions.push(vscode.workspace.onWillSaveTextDocument(e => {
		const formatterPath = findBindingsFormatter(e.document);
		if (formatterPath)
			e.waitUntil(formatBindingsDocument(e.document, formatterPath));
	}));

	vscode.languages.registerDocumentFormattingEditProvider('zscript', {
        async provideDocumentFormattingEdits(document: vscode.TextDocument): Promise<vscode.TextEdit[]> {
			// const settings = vscode.workspace.getConfiguration('zscript', document.uri);

			const formatterPath = findBindingsFormatter(document);
			if (formatterPath)
				return await formatBindingsDocument(document, formatterPath);

			try {
				const text = document.getText();
				const args = [
					'-style=file',
				];
				const parentDir = path.dirname(document.uri.fsPath);
				const output = await new Promise<string>((resolve, reject) => {
					const cp = childProcess.execFile('clang-format', args, {cwd: parentDir}, (err, stdout, stderr) => {
						if (err)
							return reject(err);

						resolve(stdout);
					});
					cp.stdin.write(text);
					cp.stdin.end();
				});
				if (output && text !== output) {
					const r = new vscode.Range(document.lineAt(0).range.start, document.lineAt(document.lineCount - 1).range.end);
					return [vscode.TextEdit.replace(r, output)];
				}
			} catch (e) {
				vscode.window.showErrorMessage(`Error while attempting to format script:\n${e.message}`);
				console.log(e);
			}
        }
    });

	// vscode.languages.registerReferenceProvider('zscript', {
	// 	provideReferences(document, position, context, token) {
	// 		console.log({token});
	// 		return null;
	// 	},
	// });

	// The server is implemented in node
	const serverModule = context.asAbsolutePath(
		path.join('server', 'out', 'server.js')
	);

	// If the extension is launched in debug mode then the debug server options are used
	// Otherwise the run options are used
	const serverOptions: ServerOptions = {
		run: { module: serverModule, transport: TransportKind.ipc },
		debug: {
			module: serverModule,
			transport: TransportKind.ipc,
		},
	};

	// Options to control the language client
	const clientOptions: LanguageClientOptions = {
		// Register the server for plain text documents
		documentSelector: [{ scheme: 'file', pattern: '**/*.{zs,zh,z}' }],
		synchronize: {
			fileEvents: vscode.workspace.createFileSystemWatcher('**/*.{zs,zh,z}')
		},
		markdown: {
			isTrusted: true,
		},
	};

	// Create the language client and start the client.
	client = new LanguageClient(
		'zscriptLanguageServer',
		'ZScript Language Server',
		serverOptions,
		clientOptions
	);

	const command = {
		id: 'zscript.openLink',
		isTextEditorCommand: false,
		async execute(args) {
			return await vscode.commands.executeCommand('vscode.open', vscode.Uri.parse(args.file), <vscode.TextDocumentShowOptions>{
				selection: new vscode.Range(
					new vscode.Position(
						args.position?.start.line ?? 0, args.position?.start.character ?? 0),
					new vscode.Position(
						args.position?.end.line ?? 0, args.position?.end.character ?? 0)),
			});
		},
	};
	const disposable = command.isTextEditorCommand ?
        vscode.commands.registerTextEditorCommand(command.id, command.execute) :
        vscode.commands.registerCommand(command.id, command.execute);
    context.subscriptions.push(disposable);

	// Start the client. This will also launch the server
	client.start();
}

export function deactivate(): Thenable<void> | undefined {
	if (!client) {
		return undefined;
	}
	return client.stop();
}
