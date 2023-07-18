import {
	createConnection,
	TextDocuments,
	Diagnostic,
	DiagnosticSeverity,
	ProposedFeatures,
	InitializeParams,
	DidChangeConfigurationNotification,
	CompletionItem,
	CompletionItemKind,
	TextDocumentPositionParams,
	TextDocumentSyncKind,
	InitializeResult
} from 'vscode-languageserver/node';

import {
	TextDocument
} from 'vscode-languageserver-textdocument';
import * as childProcess from 'child_process';
import {promisify} from 'util';
import * as os from 'os';
import * as fs from 'fs';

const execFile = promisify(childProcess.execFile);

// Create a connection for the server, using Node's IPC as a transport.
// Also include all preview / proposed LSP features.
const connection = createConnection(ProposedFeatures.all);

// Create a simple text document manager.
const documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);

let hasConfigurationCapability = false;
let hasWorkspaceFolderCapability = false;
let hasDiagnosticRelatedInformationCapability = false;

connection.onInitialize((params: InitializeParams) => {
	const capabilities = params.capabilities;

	// Does the client support the `workspace/configuration` request?
	// If not, we fall back using global settings.
	hasConfigurationCapability = !!(
		capabilities.workspace && !!capabilities.workspace.configuration
	);
	hasWorkspaceFolderCapability = !!(
		capabilities.workspace && !!capabilities.workspace.workspaceFolders
	);
	hasDiagnosticRelatedInformationCapability = !!(
		capabilities.textDocument &&
		capabilities.textDocument.publishDiagnostics &&
		capabilities.textDocument.publishDiagnostics.relatedInformation
	);

	const result: InitializeResult = {
		capabilities: {
			textDocumentSync: TextDocumentSyncKind.Incremental,
			// Tell the client that this server supports code completion.
			completionProvider: {
				resolveProvider: true
			}
		}
	};
	if (hasWorkspaceFolderCapability) {
		result.capabilities.workspace = {
			workspaceFolders: {
				supported: true
			}
		};
	}
	return result;
});

connection.onInitialized(() => {
	if (hasConfigurationCapability) {
		// Register for all configuration changes.
		connection.client.register(DidChangeConfigurationNotification.type, undefined);
	}
	if (hasWorkspaceFolderCapability) {
		connection.workspace.onDidChangeWorkspaceFolders(_event => {
			connection.console.log('Workspace folder change event received.');
		});
	}
});

// The example settings
interface Settings {
	installationFolder?: string;
	printCompilerOutput?: boolean;
}

// The global settings, used when the `workspace/configuration` request is not supported by the client.
// Please note that this is not the case when using this server with the client provided in this example
// but could happen with other clients.
const defaultSettings: Settings = { };
let globalSettings: Settings = defaultSettings;

// Cache the settings of all open documents
const documentSettings: Map<string, Thenable<Settings>> = new Map();

connection.onDidChangeConfiguration(change => {
	if (hasConfigurationCapability) {
		// Reset all cached document settings
		documentSettings.clear();
	} else {
		globalSettings = <Settings>(
			(change.settings.zscriptLanguageServer || defaultSettings)
		);
	}

	// Revalidate all open text documents
	documents.all().forEach(processScript);
});

function getDocumentSettings(resource: string): Thenable<Settings> {
	if (!hasConfigurationCapability) {
		return Promise.resolve(globalSettings);
	}
	let result = documentSettings.get(resource);
	if (!result) {
		result = connection.workspace.getConfiguration({
			scopeUri: resource,
			section: 'zscript'
		});
		documentSettings.set(resource, result);
	}
	return result;
}

// Only keep settings for open documents
documents.onDidClose(e => {
	documentSettings.delete(e.document.uri);
});

// The content of a text document has changed. This event is emitted
// when the text document first opened or when its content has changed.
documents.onDidChangeContent(change => {
	processScript(change.document);
});

let globalTmpDir = '';

async function processScript(textDocument: TextDocument): Promise<void> {
	// In this simple example we get the settings for every validate run.
	const settings = await getDocumentSettings(textDocument.uri);

	// The validator creates diagnostics for all uppercase words length 2 and more
	const text = textDocument.getText();

	if (!settings.installationFolder) {
		connection.sendDiagnostics({ uri: textDocument.uri, diagnostics: [{
			severity: DiagnosticSeverity.Error,
			range: {
				start: textDocument.positionAt(0),
				end: textDocument.positionAt(0),
			},
			message: 'Must set zscript.installationFolder setting',
			source: 'extension'
		}]});
		return;
	}

	if (!globalTmpDir) {
		globalTmpDir = os.tmpdir();
	}

	const tmpInput = `${globalTmpDir}/tmp.zs`;
	let stdout = '';
	let success = false;
	fs.writeFileSync(tmpInput, text);
	const exe = os.platform() === 'win32' ? './zscript.exe' : './zscript';
	try {
		const cp = await execFile(exe, [
			'-unlinked',
			'-input', tmpInput,
		], {
			cwd: settings.installationFolder,
		});
		success = true;
		stdout = cp.stdout;
	} catch (e: any) {
		if (e.code === undefined) throw e;
		stdout = e.stdout || e.toString();
	}
	if (settings.printCompilerOutput) {
		console.log(stdout);
	}

	
	const diagnostics: Diagnostic[] = [];
	for (const line of stdout.split('\n')) {
		if (line.includes('syntax error')) {
			const m = line.match(/syntax error, (.*) \[.*Line (\d+) Column (\d+).*\].*/);
			let message = '';
			let lineNum = 0;
			let colNum = 0;
			if (m) {
				message = m[1];
				lineNum = Number(m[2]) - 1;
				colNum = Number(m[3]);
			} else {
				message = line.split('syntax error, ', 2)[1];
			}

			const start = textDocument.offsetAt({line: lineNum, character: 0});
			const end = textDocument.offsetAt({line: lineNum, character: colNum});
			const diagnostic: Diagnostic = {
				severity: DiagnosticSeverity.Error,
				range: {
					start: textDocument.positionAt(start),
					end: textDocument.positionAt(end),
				},
				message,
				source: exe,
			};
			diagnostics.push(diagnostic);
		} else if (line.includes('Error') || line.includes('Warning')) {
			const m = line.match(/.*Line (\d+).*Columns (\d+)-(\d+) - (.*)/);
			let message = '';
			let lineNum = 0;
			let colStartNum = 0;
			let colEndNum = 0;
			if (m) {
				lineNum = Number(m[1]) - 1;
				colStartNum = Number(m[2]) - 1;
				colEndNum = Number(m[3]) - 1;
				message = m[4];
			} else {
				message = line;
			}

			const start = textDocument.offsetAt({line: lineNum, character: colStartNum});
			const end = textDocument.offsetAt({line: lineNum, character: colEndNum});
			const diagnostic: Diagnostic = {
				severity: message.startsWith('Error') ? DiagnosticSeverity.Error : DiagnosticSeverity.Warning,
				range: {
					start: textDocument.positionAt(start),
					end: textDocument.positionAt(end),
				},
				message,
				source: exe,
			};
			diagnostics.push(diagnostic);
		}
	}

	// Fallback, incase compiling failed but we failed to parse out an error.
	if (!success && diagnostics.length === 0) {
		diagnostics.push({
			severity: DiagnosticSeverity.Error,
			range: {
				start: textDocument.positionAt(0),
				end: textDocument.positionAt(0),
			},
			message: stdout,
			source: exe,
		});
	}

	// Send the computed diagnostics to VSCode.
	connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
}

connection.onDidChangeWatchedFiles(_change => {
	// Monitored files have change in VSCode
	connection.console.log('We received an file change event');
});

// This handler provides the initial list of the completion items.
connection.onCompletion(
	(_textDocumentPosition: TextDocumentPositionParams): CompletionItem[] => {
		return [];
		// The pass parameter contains the position of the text document in
		// which code complete got requested. For the example we ignore this
		// info and always provide the same completion items.
		// return [
		// 	{
		// 		label: 'TypeScript',
		// 		kind: CompletionItemKind.Text,
		// 		data: 1
		// 	},
		// 	{
		// 		label: 'JavaScript',
		// 		kind: CompletionItemKind.Text,
		// 		data: 2
		// 	}
		// ];
	}
);

// This handler resolves additional information for the item selected in
// the completion list.
connection.onCompletionResolve(
	(item: CompletionItem): CompletionItem => {
		// if (item.data === 1) {
		// 	item.detail = 'TypeScript details';
		// 	item.documentation = 'TypeScript documentation';
		// } else if (item.data === 2) {
		// 	item.detail = 'JavaScript details';
		// 	item.documentation = 'JavaScript documentation';
		// }
		return item;
	}
);

// Make the text document manager listen on the connection
// for open, change and close text document events
documents.listen(connection);

// Listen on the connection
connection.listen();