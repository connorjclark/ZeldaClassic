import * as vscode from 'vscode';
import * as assert from 'assert';
import { getDocUri, activate, setVersion } from './helper.js';
import { before } from 'mocha';

function range(sLine: number, sChar: number, eLine: number, eChar: number) {
	const start = new vscode.Position(sLine, sChar);
	const end = new vscode.Position(eLine, eChar);
	return new vscode.Range(start, end);
}

async function testDiagnostics(docUri: vscode.Uri, expectedDiagnostics: vscode.Diagnostic[]) {
	await activate(docUri);

	const actualDiagnostics = vscode.languages.getDiagnostics(docUri)
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
	const docUri = getDocUri('diagnostics.zs');

	test('2.55', async () => {
		await setVersion('2.55');
		await testDiagnostics(docUri, [
			{ message: `Warning S094: Variable 'Screen->HasItem' is deprecated, and should not be used.`, range: range(5, 1, 5, 16), severity: vscode.DiagnosticSeverity.Warning },
		]);
	});

	test('3.0', async () => {
		await setVersion('3');
		await testDiagnostics(docUri, [
			{ message: `S094: Variable 'screendata->HasItem' is deprecated, and should not be used.\nCheck '->Item > -1' instead!`, range: range(5, 1, 5, 16), severity: vscode.DiagnosticSeverity.Warning },
			{ message: `S102: Function 'fn' is not void, and must return a value!`, range: range(4, 4, 4, 6), severity: vscode.DiagnosticSeverity.Error },
		]);
	});
});

suite('Diagnoses parser errors', () => {
	const docUri = getDocUri('parser_errors.zs');

	test('2.55', async () => {
		await setVersion('2.55');
		await testDiagnostics(docUri, [
			{ message: `unexpected LBRACE`, range: range(2, 22, 2, 23), severity: vscode.DiagnosticSeverity.Error },
			{ message: `Error in temp file`, range: range(0, 0, 0, 0), severity: vscode.DiagnosticSeverity.Error },
		]);
	});

	test('3.0', async () => {
		await setVersion('3');
		await testDiagnostics(docUri, [
			{ message: `syntax error, unexpected LBRACE`, range: range(2, 22, 2, 23), severity: vscode.DiagnosticSeverity.Error },
		]);
	});
});

suite('Default includes', () => {
	const docUri = getDocUri('default_includes.zs');

	before(async () => {
		const config = vscode.workspace.getConfiguration('zscript');
		await config.update('defaultIncludeFiles', ['std.zh'], vscode.ConfigurationTarget.Global);
	});

	test('2.55', async () => {
		await setVersion('2.55');
		await testDiagnostics(docUri, []);
	});

	test('3.0', async () => {
		await setVersion('3');
		await testDiagnostics(docUri, []);
	});
});
