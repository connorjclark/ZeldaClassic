const DiagnosticSeverity = {
	/**
	 * Reports an error.
	 */
	Error: 1,
	/**
	 * Reports a warning.
	 */
	Warning: 2,
	/**
	 * Reports an information.
	 */
	Information: 3,
	/**
	 * Reports a hint.
	 */
	Hint: 4,
};

function cleanupFile(file) {
	return file;
}

function fileMatches(f1, f2) {
	return cleanupFile(f1) == cleanupFile(f2);
}

function parseForDiagnostics(output) {
	const diagnostics = [];
	for (const line of output.split('\n')) {
		if (line.includes('syntax error')) {
			const m = line.match(/syntax error, (.*) \[(.*) Line (\d+) Column (\d+).*\].*/);
			let message = '';
			let lineNum = 0;
			let colNum = 0;
			let fname = '';
			if (m) {
				fname = cleanupFile(m[2]);
				message = m[1].trim();
				if (fileMatches(fname, "ZQ_BUFFER")) {
					lineNum = 0;
					colNum = 0;
					message = `Syntax error in temp file (check your ZScript Extension settings):\n${message}`;
				}
				else// if (fileMatches(fname, tmpScript))
				{
					lineNum = Number(m[3]) - 1;
					colNum = Number(m[4]);
				}
				// else {
				// 	lineNum = 0;
				// 	colNum = 0;
				// 	message = `Syntax error in "${fname}":\n${message}`;
				// }
			} else {
				message = line.split('syntax error, ', 2)[1].trim();
			}

			// const start = textDocument.offsetAt({line: lineNum, character: 0});
			// const end = textDocument.offsetAt({line: lineNum, character: colNum});
			// const diagnostic = {
			// 	severity: DiagnosticSeverity.Error,
			// 	range: {
			// 		start: textDocument.positionAt(start),
			// 		end: textDocument.positionAt(end),
			// 	},
			// 	message,
			// 	source: exe,
			// };
			// diagnostics.push(diagnostic);
			const start = ({ line: lineNum, character: 0 });
			const end = ({ line: lineNum, character: colNum });
			const diagnostic = {
				severity: DiagnosticSeverity.Error,
				range: {
					start: (start),
					end: (end),
				},
				message,
				source: 'zscript',
			};
			diagnostics.push(diagnostic);
		} else if (line.includes('Error') || line.includes('Warning')) {
			const m = line.match(/(.*)Line (\d+).*Columns (\d+)-(\d+) - (.*)/);
			let message = '';
			let lineNum = 0;
			let colStartNum = 0;
			let colEndNum = 0;
			let sev = 'Warning';
			let fname = '';
			if (m) {
				fname = cleanupFile(m[1]);
				message = m[5].trim();
				if (message.startsWith('Error'))
					sev = 'Error';
				if (fname.length == 0 || fileMatches(fname, "ZQ_BUFFER")) //error in temp file
				{
					lineNum = 0;
					colStartNum = 0;
					colEndNum = 0;
					message = `${sev} in temp file (check your ZScript Extension settings):\n${message}`;
				}
				else
				{
					lineNum = Number(m[2]) - 1;
					colStartNum = Number(m[3]) - 1;
					colEndNum = Number(m[4]) - 1;
				}
				// else {
				// 	lineNum = 0;
				// 	colStartNum = 0;
				// 	colEndNum = 0;
				// 	message = `${sev} in "${fname}":\n${message}`;
				// }
			} else {
				message = line.trim();
			}

			const start = ({ line: lineNum, character: colStartNum });
			const end = ({ line: lineNum, character: colEndNum });
			const diagnostic = {
				severity: sev == 'Error' ? DiagnosticSeverity.Error : DiagnosticSeverity.Warning,
				range: {
					start: (start),
					end: (end),
				},
				message,
				source: 'zscript',
			};
			diagnostics.push(diagnostic);
		} else if (line.includes('ERROR:') || line.includes('WARNING:')) {
			const m = line.match(/(.*) \[(.*) Line (\d+) Column (\d+).*/);
			let message = '';
			let lineNum = 0;
			let colNum = 0;
			let sev = 'Warning';
			let fname = '';
			if (m) {
				fname = cleanupFile(m[2]);
				message = m[1].trim();
				if (message.startsWith('ERROR:'))
					sev = 'Error';
				if (fileMatches(fname, "ZQ_BUFFER")) {
					lineNum = 0;
					colNum = 0;
					message = `${sev} in temp file (check your ZScript Extension settings):\n${message}`;
				}
				else// if (fileMatches(fname, tmpScript))
				{
					lineNum = Number(m[3]) - 1;
					colNum = Number(m[4]);
				}
				// else {
				// 	lineNum = 0;
				// 	colNum = 0;
				// 	message = `${sev} in "${fname}":\n${message}`;
				// }
			} else {
				message = line.trim();
			}

			// const start = textDocument.offsetAt({ line: lineNum, character: 0 });
			// const end = textDocument.offsetAt({ line: lineNum, character: colNum });
			// const diagnostic = {
			// 	severity: sev=='Error' ? DiagnosticSeverity.Error : DiagnosticSeverity.Warning,
			// 	range: {
			// 		start: textDocument.positionAt(start),
			// 		end: textDocument.positionAt(end),
			// 	},
			// 	message,
			// 	source: exe,
			// };
			const start = ({ line: lineNum, character: 0 });
			const end = ({ line: lineNum, character: colNum });
			const diagnostic = {
				severity: sev == 'Error' ? DiagnosticSeverity.Error : DiagnosticSeverity.Warning,
				range: {
					start,
					end,
				},
				message,
				source: 'zscript',
			};
			diagnostics.push(diagnostic);
		}
	}

	return diagnostics;
}

export function parseZscriptOutput(output) {
	const diagnostics = parseForDiagnostics(output);
	const metadata = null;

	return { diagnostics, metadata };
}
