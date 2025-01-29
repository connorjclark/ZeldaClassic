import * as vscode from 'vscode';
import * as assert from 'assert';
import { getDocUri, activate, setVersion, setTestContent, executeDocumentSymbolProvider } from './helper.js';
import { before } from 'mocha';

function range(sLine: number, sChar: number, eLine: number, eChar: number) {
	const start = new vscode.Position(sLine, sChar);
	const end = new vscode.Position(eLine, eChar);
	return new vscode.Range(start, end);
}

async function testDiagnostics(uri: vscode.Uri, expectedDiagnostics: vscode.Diagnostic[]) {
	await activate(uri);

	const actualDiagnostics = vscode.languages.getDiagnostics(uri)
		.map(({ message, range, severity }) => ({ message, range, severity }));
	for (const diag of actualDiagnostics) {
		if (diag.message.startsWith('Error in temp file')) {
			diag.message = 'Error in temp file';
		}
	}
	assert.deepStrictEqual(actualDiagnostics, expectedDiagnostics);
}

before(async () => {
	const config = vscode.workspace.getConfiguration('zscript');
	await config.update('installationFolder', null, vscode.ConfigurationTarget.Global);
	await config.update('defaultIncludeFiles', [], vscode.ConfigurationTarget.Global);
});

suite('Diagnoses errors and warnings', () => {
	const uri = getDocUri('diagnostics.zs');

	test('2.55', async () => {
		await setVersion('2.55');
		await testDiagnostics(uri, [
			{ message: `Warning S094: Variable 'Screen->HasItem' is deprecated, and should not be used.`, range: range(5, 1, 5, 16), severity: vscode.DiagnosticSeverity.Warning },
		]);
	});

	test('latest', async () => {
		await setVersion('latest');
		await testDiagnostics(uri, [
			{ message: `S094: Variable 'screendata->HasItem' is deprecated, and should not be used.\nCheck \`->Item > -1\` instead!`, range: range(5, 1, 5, 16), severity: vscode.DiagnosticSeverity.Warning },
			{ message: `S102: Function 'fn' is not void, and must return a value!`, range: range(4, 4, 4, 6), severity: vscode.DiagnosticSeverity.Error },
		]);
	});
});

suite('Diagnoses errors and warnings - unsaved changes', () => {
	const uri = getDocUri('empty.zs');

	before(async () => {
		await setTestContent(uri, '#option WARN_DEPRECATED warn\n\nimport "std.zh"\n\nvoid fn() {\n\tScreen->HasItem;\n}\n');
	});

	test('2.55', async () => {
		await setVersion('2.55');
		await testDiagnostics(uri, [
			{ message: `Warning S094: Variable 'Screen->HasItem' is deprecated, and should not be used.`, range: range(5, 1, 5, 16), severity: vscode.DiagnosticSeverity.Warning },
		]);
	});

	test('latest', async () => {
		await setVersion('latest');
		await testDiagnostics(uri, [
			{ message: `S094: Variable 'screendata->HasItem' is deprecated, and should not be used.\nCheck \`->Item > -1\` instead!`, range: range(5, 1, 5, 16), severity: vscode.DiagnosticSeverity.Warning },
		]);
	});
});

suite('Diagnoses parser errors', () => {
	const uri = getDocUri('parser_errors.zs');

	test('2.55', async () => {
		await setVersion('2.55');
		await testDiagnostics(uri, [
			{ message: `unexpected LBRACE`, range: range(2, 22, 2, 23), severity: vscode.DiagnosticSeverity.Error },
			{ message: `Error in temp file`, range: range(0, 0, 0, 0), severity: vscode.DiagnosticSeverity.Error },
		]);
	});

	test('latest', async () => {
		await setVersion('latest');
		await testDiagnostics(uri, [
			{ message: `syntax error, unexpected LBRACE`, range: range(2, 22, 2, 23), severity: vscode.DiagnosticSeverity.Error },
		]);
	});
});

suite('Default includes', () => {
	const uri = getDocUri('default_includes.zs');

	before(async () => {
		const config = vscode.workspace.getConfiguration('zscript');
		await config.update('defaultIncludeFiles', ['std.zh'], vscode.ConfigurationTarget.Global);
	});

	test('2.55', async () => {
		await setVersion('2.55');
		await testDiagnostics(uri, []);
	});

	test('latest', async () => {
		await setVersion('latest');
		await testDiagnostics(uri, []);
	});
});

suite.only('Document symbol outline', () => {
	const uri = getDocUri('symbols.zs');

	test('latest', async () => {
		await setVersion('latest');

		await new Promise(resolve => setTimeout(resolve, 2000));
		const symbols = await executeDocumentSymbolProvider(uri);
		
	});

	// test('latest', async () => {
	// 	await setVersion('latest');
	// 	await testDiagnostics(uri, []);
	// });
});

// TODO ! document symbols