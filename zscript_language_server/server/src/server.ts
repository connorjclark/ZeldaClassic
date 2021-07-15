import * as childProcess from 'child_process';
import * as crypto from 'crypto';
import {tmpdir} from 'os';
import * as path from 'path';
import * as fs from 'fs';

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
	InitializeResult,
	Range
} from 'vscode-languageserver/node';

import {
	TextDocument
} from 'vscode-languageserver-textdocument';
import { promisify } from 'util';

const exec = promisify(childProcess.exec);

function tmpFile(ext: string) {
    return path.join(tmpdir(),`archive.${crypto.randomBytes(6).readUIntLE(0,6).toString(36)}.${ext}`);
}

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

interface Settings {
	zeldaClassicDirectory: string;
}

// The global settings, used when the `workspace/configuration` request is not supported by the client.
// Please note that this is not the case when using this server with the client provided in this example
// but could happen with other clients.
const defaultSettings: Settings = {
	zeldaClassicDirectory: '',
};
let globalSettings: Settings = defaultSettings;

const zscriptConstants: Map<string, Array<{name: string, documentation: string}>> = new Map();

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

	// Revalidate all open documents
	documents.all().forEach(validateScript);
});

async function getDocumentSettings(resource: string): Promise<Settings> {
	if (!hasConfigurationCapability) {
		return Promise.resolve(globalSettings);
	}
	let result = documentSettings.get(resource);
	if (!result) {
		result = connection.workspace.getConfiguration({
			scopeUri: resource,
			section: 'zscriptLanguageServer'
		});
		documentSettings.set(resource, result);
	}
	return result;
}

// TODO: should figure out how to get this data from zscript.exe so that all
//       constants are available, not just the ones defined in std_constants.zh
async function getZscriptConstants(zeldaClassicDirectory: string) {
	let constants = zscriptConstants.get(zeldaClassicDirectory);
	if (constants) return constants;

	constants = [];
	const stdConstantsPath = `${zeldaClassicDirectory}/std_zh/std_constants.zh`;
	if (fs.existsSync(stdConstantsPath)) {
		const constantsScript = await fs.promises.readFile(stdConstantsPath, 'utf-8');
		const lines = constantsScript.split("\n");
		for (const line of lines) {
			const match = line.match(/const \w* (\w*).*\/\/(.*)/);
			if (!match) continue;
	
			const [, name, documentation] = match;
			constants.push({
				name,
				documentation,
			});
		}
	}
	zscriptConstants.set(zeldaClassicDirectory, constants);
	return constants;
}

// Only keep settings for open documents
documents.onDidClose(e => {
	documentSettings.delete(e.document.uri);
});
documents.onDidOpen(change => {
	validateScript(change.document);
});
documents.onDidSave(change => {
	validateScript(change.document);
});

async function validateScript(textDocument: TextDocument): Promise<void> {
	const settings = await getDocumentSettings(textDocument.uri);

	if (!settings.zeldaClassicDirectory) {
		connection.sendDiagnostics({ uri: textDocument.uri, diagnostics: [{
			severity: DiagnosticSeverity.Error,
			range: Range.create(0, 0, 0, 0),
			message: 'You must configure the `zeldaClassicDirectory` setting in VS Code',
			source: 'zscript-language-server',
		}] });
		return;
	}

	const cwd = settings.zeldaClassicDirectory;
	const tmpScriptPath = tmpFile('zs');
	fs.writeFileSync(tmpScriptPath, textDocument.getText());
	// const scriptPath = textDocument.uri.replace('file://', '');
	// const result = execFileSync('zscript.exe', [
	// 	'-input', tmpScriptPath,
	// ], {shell: true, encoding: 'utf-8', cwd});
	const {stdout, stderr} = await exec(`zscript.exe -input "${tmpScriptPath}"`, {encoding: 'utf-8', cwd});
	const lines = [...stdout.split("\n"), ...stderr.split("\n")];

	const diagnostics: Diagnostic[] = [];
	for (const line of lines) {
		const isError = line.match(/error/i);
		const isWarning = line.match(/warning/i);
		if (!isError && !isWarning) continue;

		let startLine = 0;
		let startColumn = 0;
		let endColumn = 0;

		let match = line.match(/Line (\d+).*Columns (\d+)-(\d+)/i);
		if (match) {
			startLine = Number(match[1]) - 1;
			startColumn = Number(match[2]) - 1;
			endColumn = Number(match[3]) - 1;
		}

		match = line.match(/Line (\d+) Column (\d+)/i);
		if (match) {
			startLine = Number(match[1]) - 1;
			startColumn = Number(match[2]) - 1;
			endColumn = startColumn;
		}

		const diagnostic: Diagnostic = {
			severity: isError ? DiagnosticSeverity.Error : DiagnosticSeverity.Warning,
			range: Range.create(startLine, startColumn, startLine, endColumn),
			message: line,
			source: 'zscript',
		};
		diagnostics.push(diagnostic);
	}

	connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
}

connection.onDidChangeWatchedFiles(_change => {
	// Monitored files have change in VSCode
	connection.console.log('We received an file change event');
});

// This handler provides the initial list of the completion items.
connection.onCompletion(
	async (_textDocumentPosition: TextDocumentPositionParams): Promise<CompletionItem[]> => {
		const settings = await getDocumentSettings(_textDocumentPosition.textDocument.uri);
		const constants = await getZscriptConstants(settings.zeldaClassicDirectory);
		// The pass parameter contains the position of the text document in
		// which code complete got requested. For the example we ignore this
		// info and always provide the same completion items.
		return constants.map((constant) => {
			return {
				label: constant.name,
				detail: constant.documentation,
				documentation: constant.documentation,
				kind: CompletionItemKind.Variable,
			};
		});
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
