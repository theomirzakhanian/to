import * as vscode from 'vscode';
import * as cp from 'child_process';
import * as path from 'path';

let diagnosticCollection: vscode.DiagnosticCollection;

// Built-in keywords and functions for autocomplete
const KEYWORDS = [
    'to', 'if', 'or', 'else', 'given', 'through', 'as', 'while',
    'build', 'my', 'const', 'use', 'from', 'try', 'catch', 'finally',
    'return', 'print', 'and', 'not', 'true', 'false', 'none',
    'break', 'continue', 'in', 'async', 'await'
];

const BUILTIN_FUNCTIONS = [
    { name: 'print', detail: 'print value', doc: 'Print a value to stdout' },
    { name: 'input', detail: 'input(prompt) -> string', doc: 'Read a line from stdin' },
    { name: 'len', detail: 'len(x) -> int', doc: 'Length of string, list, or dict' },
    { name: 'type', detail: 'type(x) -> string', doc: 'Type name of a value' },
    { name: 'int', detail: 'int(x) -> int', doc: 'Convert to integer' },
    { name: 'float', detail: 'float(x) -> float', doc: 'Convert to float' },
    { name: 'str', detail: 'str(x) -> string', doc: 'Convert to string' },
    { name: 'range', detail: 'range(start, end) -> list', doc: 'Create a range list' },
    { name: 'abs', detail: 'abs(x) -> number', doc: 'Absolute value' },
    { name: 'min', detail: 'min(a, b) -> number', doc: 'Minimum of two values' },
    { name: 'max', detail: 'max(a, b) -> number', doc: 'Maximum of two values' },
    { name: 'round', detail: 'round(x) -> int', doc: 'Round to nearest integer' },
    { name: 'read_file', detail: 'read_file(path) -> string', doc: 'Read file contents' },
    { name: 'write_file', detail: 'write_file(path, content)', doc: 'Write to file' },
];

const MODULES = [
    { name: 'math', detail: 'Math functions', doc: 'pi, e, sqrt, sin, cos, floor, ceil, pow' },
    { name: 'io', detail: 'File I/O', doc: 'read(path), write(path, content)' },
    { name: 'web', detail: 'HTTP server', doc: 'serve(port, handler), json(), parse_json()' },
    { name: 'json', detail: 'JSON utilities', doc: 'stringify(value), parse(string)' },
    { name: 'ffi', detail: 'C FFI', doc: 'open(lib), sizeof(type)' },
    { name: 'time', detail: 'Date & time', doc: 'now(), ms(), sleep(), date(), clock(), year(), month(), day()' },
    { name: 'fs', detail: 'Filesystem', doc: 'read(), write(), exists(), list(), walk(), mkdir(), remove(), copy()' },
    { name: 'regex', detail: 'Regular expressions', doc: 'match(), find_all(), replace(), split(), test()' },
];

const TYPE_HINTS = ['int', 'float', 'number', 'string', 'str', 'bool', 'list', 'dict', 'any', 'none'];

const LIST_METHODS = ['add', 'remove', 'pop', 'contains', 'join', 'reverse', 'length'];
const STRING_METHODS = ['upper', 'lower', 'trim', 'split', 'contains', 'replace', 'starts_with', 'ends_with', 'length'];
const DICT_METHODS = ['keys', 'values', 'has'];

export function activate(context: vscode.ExtensionContext) {
    diagnosticCollection = vscode.languages.createDiagnosticCollection('to');
    context.subscriptions.push(diagnosticCollection);

    // Run diagnostics on save and open
    const config = vscode.workspace.getConfiguration('to');

    if (config.get<boolean>('enableDiagnostics', true)) {
        context.subscriptions.push(
            vscode.workspace.onDidSaveTextDocument(doc => {
                if (doc.languageId === 'to') runDiagnostics(doc);
            })
        );
        context.subscriptions.push(
            vscode.workspace.onDidOpenTextDocument(doc => {
                if (doc.languageId === 'to') runDiagnostics(doc);
            })
        );
        context.subscriptions.push(
            vscode.workspace.onDidChangeTextDocument(e => {
                if (e.document.languageId === 'to') {
                    // Debounce — run after 500ms of no typing
                    if ((runDiagnostics as any)._timer) clearTimeout((runDiagnostics as any)._timer);
                    (runDiagnostics as any)._timer = setTimeout(() => runDiagnostics(e.document), 500);
                }
            })
        );
    }

    // Autocomplete provider
    const completionProvider = vscode.languages.registerCompletionItemProvider('to', {
        provideCompletionItems(document, position) {
            const items: vscode.CompletionItem[] = [];
            const lineText = document.lineAt(position).text;
            const linePrefix = lineText.substring(0, position.character);

            // After 'use ' — suggest modules
            if (linePrefix.match(/use\s+$/)) {
                for (const mod of MODULES) {
                    const item = new vscode.CompletionItem(mod.name, vscode.CompletionItemKind.Module);
                    item.detail = mod.detail;
                    item.documentation = mod.doc;
                    items.push(item);
                }
                return items;
            }

            // After ':' in param — suggest types
            if (linePrefix.match(/\w+\s*:\s*$/)) {
                for (const t of TYPE_HINTS) {
                    const item = new vscode.CompletionItem(t, vscode.CompletionItemKind.TypeParameter);
                    item.detail = `Type hint: ${t}`;
                    items.push(item);
                }
                return items;
            }

            // After '->' — suggest return types
            if (linePrefix.match(/->\s*$/)) {
                for (const t of TYPE_HINTS) {
                    const item = new vscode.CompletionItem(t, vscode.CompletionItemKind.TypeParameter);
                    items.push(item);
                }
                return items;
            }

            // After '.' — suggest methods based on context
            if (linePrefix.match(/\.\s*$/)) {
                // Suggest all method types
                for (const m of LIST_METHODS) {
                    const item = new vscode.CompletionItem(m, vscode.CompletionItemKind.Method);
                    item.detail = `list.${m}`;
                    items.push(item);
                }
                for (const m of STRING_METHODS) {
                    const item = new vscode.CompletionItem(m, vscode.CompletionItemKind.Method);
                    item.detail = `string.${m}`;
                    items.push(item);
                }
                for (const m of DICT_METHODS) {
                    const item = new vscode.CompletionItem(m, vscode.CompletionItemKind.Method);
                    item.detail = `dict.${m}`;
                    items.push(item);
                }
                return items;
            }

            // Keywords
            for (const kw of KEYWORDS) {
                const item = new vscode.CompletionItem(kw, vscode.CompletionItemKind.Keyword);
                items.push(item);
            }

            // Built-in functions
            for (const fn of BUILTIN_FUNCTIONS) {
                const item = new vscode.CompletionItem(fn.name, vscode.CompletionItemKind.Function);
                item.detail = fn.detail;
                item.documentation = fn.doc;
                items.push(item);
            }

            // Scan document for user-defined names
            const text = document.getText();

            // Functions
            const funcMatches = text.matchAll(/\bto\s+(\w+)\s*\(/g);
            for (const match of funcMatches) {
                const item = new vscode.CompletionItem(match[1], vscode.CompletionItemKind.Function);
                item.detail = 'function';
                items.push(item);
            }

            // Classes
            const classMatches = text.matchAll(/\bbuild\s+(\w+)/g);
            for (const match of classMatches) {
                const item = new vscode.CompletionItem(match[1], vscode.CompletionItemKind.Class);
                item.detail = 'class';
                items.push(item);
            }

            // Variables
            const varMatches = text.matchAll(/^(\w+)\s*=/gm);
            for (const match of varMatches) {
                if (!KEYWORDS.includes(match[1])) {
                    const item = new vscode.CompletionItem(match[1], vscode.CompletionItemKind.Variable);
                    items.push(item);
                }
            }

            return items;
        }
    }, '.', ':', ' ');
    context.subscriptions.push(completionProvider);

    // Hover provider
    const hoverProvider = vscode.languages.registerHoverProvider('to', {
        provideHover(document, position) {
            const range = document.getWordRangeAtPosition(position);
            if (!range) return;
            const word = document.getText(range);

            // Check keywords
            const keywordDocs: Record<string, string> = {
                'to': '**to** — Define a function\n```to\nto greet(name):\n  print "Hello, {name}!"\n```',
                'build': '**build** — Define a class\n```to\nbuild Person:\n  to init(name):\n    my.name = name\n```',
                'my': '**my** — Reference to current instance (like self/this)',
                'if': '**if** — Conditional branch\n```to\nif condition:\n  ...\nor other_condition:\n  ...\nelse:\n  ...\n```',
                'given': '**given** — Pattern matching\n```to\ngiven value:\n  if "a":\n    ...\n  else:\n    ...\n```',
                'through': '**through** — Loop over iterable\n```to\nthrough list as item:\n  print item\n```',
                'async': '**async** — Run a function call asynchronously\n```to\ntask = async slow_work()\nresult = await task\n```',
                'await': '**await** — Wait for an async task to complete',
                'const': '**const** — Declare an immutable variable\n```to\nconst PI = 3.14159\n```',
                'use': '**use** — Import a module\n```to\nuse math\nuse greet from "helpers.to"\n```',
                'print': '**print** value — Print to stdout',
            };

            if (keywordDocs[word]) {
                return new vscode.Hover(new vscode.MarkdownString(keywordDocs[word]));
            }

            // Check builtins
            const builtin = BUILTIN_FUNCTIONS.find(f => f.name === word);
            if (builtin) {
                return new vscode.Hover(new vscode.MarkdownString(`**${builtin.name}** — ${builtin.doc}\n\n\`${builtin.detail}\``));
            }

            // Check modules
            const mod = MODULES.find(m => m.name === word);
            if (mod) {
                return new vscode.Hover(new vscode.MarkdownString(`**${mod.name}** module — ${mod.detail}\n\nFunctions: ${mod.doc}`));
            }

            // Check user-defined functions in document
            const text = document.getText();
            const funcRegex = new RegExp(`\\bto\\s+${word}\\s*\\(([^)]*)\\)`, 'g');
            const funcMatch = funcRegex.exec(text);
            if (funcMatch) {
                const params = funcMatch[1];
                return new vscode.Hover(new vscode.MarkdownString(`**${word}**(${params}) — User-defined function`));
            }

            // Check classes
            const classRegex = new RegExp(`\\bbuild\\s+${word}`, 'g');
            if (classRegex.exec(text)) {
                return new vscode.Hover(new vscode.MarkdownString(`**${word}** — User-defined class`));
            }

            return undefined;
        }
    });
    context.subscriptions.push(hoverProvider);

    // Go to definition
    const definitionProvider = vscode.languages.registerDefinitionProvider('to', {
        provideDefinition(document, position) {
            const range = document.getWordRangeAtPosition(position);
            if (!range) return;
            const word = document.getText(range);

            const text = document.getText();

            // Find function definition
            const funcRegex = new RegExp(`\\bto\\s+${word}\\s*\\(`, 'g');
            const funcMatch = funcRegex.exec(text);
            if (funcMatch) {
                const pos = document.positionAt(funcMatch.index);
                return new vscode.Location(document.uri, pos);
            }

            // Find class definition
            const classRegex = new RegExp(`\\bbuild\\s+${word}\\b`, 'g');
            const classMatch = classRegex.exec(text);
            if (classMatch) {
                const pos = document.positionAt(classMatch.index);
                return new vscode.Location(document.uri, pos);
            }

            // Find variable assignment
            const varRegex = new RegExp(`^(${word})\\s*=`, 'gm');
            const varMatch = varRegex.exec(text);
            if (varMatch) {
                const pos = document.positionAt(varMatch.index);
                return new vscode.Location(document.uri, pos);
            }

            return undefined;
        }
    });
    context.subscriptions.push(definitionProvider);

    // Document symbol provider (outline)
    const symbolProvider = vscode.languages.registerDocumentSymbolProvider('to', {
        provideDocumentSymbols(document) {
            const symbols: vscode.DocumentSymbol[] = [];
            const text = document.getText();

            // Functions
            const funcRegex = /\bto\s+(\w+)\s*\(([^)]*)\)/g;
            let match;
            while ((match = funcRegex.exec(text)) !== null) {
                const pos = document.positionAt(match.index);
                const endPos = document.positionAt(match.index + match[0].length);
                const range = new vscode.Range(pos, endPos);
                const sym = new vscode.DocumentSymbol(
                    match[1], `(${match[2]})`,
                    vscode.SymbolKind.Function, range, range
                );
                symbols.push(sym);
            }

            // Classes
            const classRegex = /\bbuild\s+(\w+)/g;
            while ((match = classRegex.exec(text)) !== null) {
                const pos = document.positionAt(match.index);
                const endPos = document.positionAt(match.index + match[0].length);
                const range = new vscode.Range(pos, endPos);
                const sym = new vscode.DocumentSymbol(
                    match[1], 'class',
                    vscode.SymbolKind.Class, range, range
                );
                symbols.push(sym);
            }

            return symbols;
        }
    });
    context.subscriptions.push(symbolProvider);

    // Run on already open docs
    vscode.workspace.textDocuments.forEach(doc => {
        if (doc.languageId === 'to') runDiagnostics(doc);
    });

    console.log('To Language extension activated');
}

function runDiagnostics(document: vscode.TextDocument) {
    const config = vscode.workspace.getConfiguration('to');
    const toPath = config.get<string>('executablePath', 'to');

    // Write content to a temp file and run `to check`
    const text = document.getText();
    const tmpFile = `/tmp/to_check_${Date.now()}.to`;

    require('fs').writeFileSync(tmpFile, text);

    cp.exec(`${toPath} check ${tmpFile}`, { timeout: 5000 }, (err, stdout, stderr) => {
        const diagnostics: vscode.Diagnostic[] = [];
        const output = (stderr || '') + (stdout || '');

        // Parse error output: file:line:col: error: message
        const errorRegex = /(?:.*?):(\d+):(\d+):\s*error:\s*(.*?)(?:\n\s*hint:\s*(.*))?$/gm;
        let match;
        while ((match = errorRegex.exec(output)) !== null) {
            const line = parseInt(match[1]) - 1;
            const col = parseInt(match[2]) - 1;
            const message = match[3].trim();
            const hint = match[4]?.trim();

            const range = new vscode.Range(
                new vscode.Position(Math.max(0, line), Math.max(0, col)),
                new vscode.Position(Math.max(0, line), Math.max(0, col) + 10)
            );

            const diag = new vscode.Diagnostic(range, message, vscode.DiagnosticSeverity.Error);
            if (hint) {
                diag.message += `\nHint: ${hint}`;
            }
            diag.source = 'to';
            diagnostics.push(diag);
        }

        diagnosticCollection.set(document.uri, diagnostics);

        // Clean up
        try { require('fs').unlinkSync(tmpFile); } catch {}
    });
}

export function deactivate() {
    if (diagnosticCollection) {
        diagnosticCollection.dispose();
    }
}
