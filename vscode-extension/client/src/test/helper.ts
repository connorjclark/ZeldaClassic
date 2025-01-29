import * as vscode from 'vscode';
import * as path from 'path';

export let doc: vscode.TextDocument;
export let editor: vscode.TextEditor;

export async function activate(docUri: vscode.Uri) {
	const ext = vscode.extensions.getExtension('cjamcl.zquest-lsp')!;
	await ext.activate();
	doc = await vscode.workspace.openTextDocument(docUri);
	editor = await vscode.window.showTextDocument(doc);
	await sleep(2000); // Wait for server activation
}

export async function setVersion(version: string) {
	let zcPath: string;
	if (version === '2.55') {
		zcPath = process.env.ZC_PATH_255;
	} else if (version === 'latest') {
		zcPath = process.env.ZC_PATH_LATEST;
	}

	if (!zcPath) {
		throw new Error(`invalid version: ${version}`);
	}

	const config = vscode.workspace.getConfiguration('zscript');
	await config.update('installationFolder', zcPath, vscode.ConfigurationTarget.Global);
}

async function sleep(ms: number) {
	return new Promise(resolve => setTimeout(resolve, ms));
}

export const getDocPath = (p: string) => {
	return path.resolve(__dirname, '../../testFixture', p);
};
export const getDocUri = (p: string) => {
	return vscode.Uri.file(getDocPath(p));
};

export async function setTestContent(content: string): Promise<boolean> {
	const all = new vscode.Range(
		doc.positionAt(0),
		doc.positionAt(doc.getText().length)
	);
	return editor.edit(eb => eb.replace(all, content));
}
