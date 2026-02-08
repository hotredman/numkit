(function () {
    'use strict';

    var outputEl   = document.getElementById('terminal-output');
    var inputEl    = document.getElementById('terminal-input');
    var dropdownEl = document.getElementById('autocomplete-dropdown');
    var statusText = document.getElementById('status-text');
    var execTime   = document.getElementById('exec-time');
    var btnClear   = document.getElementById('btn-clear');
    var btnReset   = document.getElementById('btn-reset');
    var btnWS      = document.getElementById('btn-workspace');
    var btnExamples = document.getElementById('btn-examples');
    var examplesPanel = document.getElementById('examples-panel');
    var examplesClose = document.getElementById('examples-close');
    var examplesContainer = document.getElementById('examples-categories');

    var terminal     = new Terminal(outputEl, inputEl);
    var autocomplete = new AutocompleteManager(dropdownEl, inputEl);

    var Module  = null;
    var isReady = false;

    var multilineBuffer = '';
    var isMultiline     = false;

    // ══════════════════════════════════════
    // Examples
    // ══════════════════════════════════════

    function buildExamplesPanel() {
        examplesContainer.innerHTML = '';

        for (var c = 0; c < EXAMPLES.length; c++) {
            var cat = EXAMPLES[c];

            var catDiv = document.createElement('div');
            catDiv.className = 'example-category';

            var catTitle = document.createElement('div');
            catTitle.className = 'example-category-title';
            catTitle.textContent = cat.icon + ' ' + cat.category;
            catDiv.appendChild(catTitle);

            var itemsDiv = document.createElement('div');
            itemsDiv.className = 'example-items';

            for (var i = 0; i < cat.items.length; i++) {
                (function(item) {
                    var card = document.createElement('div');
                    card.className = 'example-card';

                    var title = document.createElement('div');
                    title.className = 'example-card-title';
                    title.textContent = item.title;
                    card.appendChild(title);

                    var desc = document.createElement('div');
                    desc.className = 'example-card-desc';
                    desc.textContent = item.description;
                    card.appendChild(desc);

                    // Preview: первые 3 строки кода
                    var preview = document.createElement('div');
                    preview.className = 'example-card-preview';
                    var previewLines = item.code.split('\n').slice(0, 3);
                    if (item.code.split('\n').length > 3) {
                        previewLines.push('...');
                    }
                    preview.textContent = previewLines.join('\n');
                    card.appendChild(preview);

                    card.addEventListener('click', function() {
                        runExample(item);
                    });

                    itemsDiv.appendChild(card);
                })(cat.items[i]);
            }

            catDiv.appendChild(itemsDiv);
            examplesContainer.appendChild(catDiv);
        }
    }

    function toggleExamples() {
        var isHidden = examplesPanel.classList.contains('hidden');
        if (isHidden) {
            examplesPanel.classList.remove('hidden');
            btnExamples.classList.add('active');
        } else {
            examplesPanel.classList.add('hidden');
            btnExamples.classList.remove('active');
        }
    }

    function closeExamples() {
        examplesPanel.classList.add('hidden');
        btnExamples.classList.remove('active');
    }

    function runExample(item) {
        if (!isReady || !Module) {
            terminal.writeError('Interpreter not ready yet.');
            return;
        }

        closeExamples();

        // Показываем заголовок
        terminal.writeSystem('--- ' + item.title + ' ---');

        // Выполняем построчно или как один блок
        var lines = item.code.split('\n');
        var block = '';
        var depth = 0;
        var openers = ['for', 'while', 'if', 'switch', 'try', 'function', 'classdef'];

        for (var i = 0; i < lines.length; i++) {
            var line = lines[i];
            var trimmed = line.trim();
            if (!trimmed) continue;

            block += (block ? '\n' : '') + line;

            // Считаем глубину блоков
            var firstWord = trimmed.split(/[\s(;,]+/)[0];
            if (openers.indexOf(firstWord) !== -1) depth++;
            if (/\bend\b/.test(trimmed)) {
                var words = trimmed.replace(/;.*$/, '').trim().split(/\s+/);
                if (words[words.length - 1] === 'end' || words[0] === 'end') {
                    depth--;
                }
            }

            // Если глубина 0 — выполняем блок
            if (depth <= 0) {
                terminal.writeInput(block);
                var result = Module.repl_execute(block);
                if (result && result.length) {
                    if (result === '__CLEAR__') {
                        terminal.clear();
                    } else if (/^Error:/m.test(result)) {
                        terminal.writeError(result);
                    } else {
                        terminal.writeLine(result);
                    }
                }
                block = '';
                depth = 0;
            }
        }

        // Если остался незавершённый блок — выполнить
        if (block) {
            terminal.writeInput(block);
            var result = Module.repl_execute(block);
            if (result && result.length) {
                if (/^Error:/m.test(result)) {
                    terminal.writeError(result);
                } else {
                    terminal.writeLine(result);
                }
            }
        }

        terminal.writeSystem('');
        terminal.focus();
    }

    // Инициализация панели
    buildExamplesPanel();

    btnExamples.addEventListener('click', toggleExamples);
    examplesClose.addEventListener('click', closeExamples);

    // ══════════════════════════════════════
    // WASM Init
    // ══════════════════════════════════════

    async function initWasm() {
        statusText.textContent = 'Loading WebAssembly...';
        console.log('[REPL] Starting WASM init...');

        try {
            if (typeof createMatlabModule === 'undefined') {
                throw new Error('createMatlabModule not found.');
            }

            console.log('[REPL] createMatlabModule found, calling...');

            Module = await createMatlabModule({
                locateFile: function(path) {
                    console.log('[REPL] locateFile:', path);
                    return path;
                },
                print: function(text) {
                    console.log('[WASM stdout]', text);
                },
                printErr: function(text) {
                    console.warn('[WASM stderr]', text);
                }
            });

            console.log('[REPL] Module loaded');

            if (typeof Module.repl_init !== 'function') {
                throw new Error('repl_init not found in module.');
            }

            var msg = Module.repl_init();
            console.log('[REPL] repl_init:', msg);
            terminal.writeSystem(msg);

            isReady = true;
            statusText.textContent = 'Ready';
            statusText.className  = 'ready';
            terminal.focus();

        } catch (err) {
            console.error('[REPL] Init failed:', err);
            statusText.textContent = 'Error: ' + err.message;
            statusText.className  = 'error';
            terminal.writeError('Failed to initialize: ' + err.message);
            initFallback();
        }
    }

    function initFallback() {
        terminal.writeSystem('Running in FALLBACK mode (no WASM).');
        terminal.writeSystem('Examples still work for demo.\n');

        Module = {
            repl_execute: function(c) {
                if (c === 'clear') return 'Workspace cleared.';
                return '[fallback] ' + c;
            },
            repl_complete: function(p) {
                var kw = ['disp','fprintf','for','while','if','end',
                    'function','clear','clc','who','zeros','ones',
                    'eye','rand','sin','cos','sqrt','plot','help'];
                return kw.filter(function(k) { return k.indexOf(p) === 0; }).join(',');
            },
            repl_reset: function()     { return 'Workspace cleared.'; },
            repl_workspace: function() { return 'No variables.'; }
        };

        isReady = true;
        statusText.textContent = 'Fallback mode';
        statusText.className  = 'ready';
        terminal.focus();
    }

    // ══════════════════════════════════════
    // Execute
    // ══════════════════════════════════════

    function executeCommand(input) {
        var trimmed = input.trim();
        if (!trimmed) { terminal.clearInput(); return; }

        if (!isReady || !Module) {
            terminal.writeError('Interpreter not ready yet.');
            terminal.clearInput();
            return;
        }

        if (needsMoreInput(trimmed) && !isMultiline) {
            isMultiline = true;
            multilineBuffer = trimmed;
            terminal.writeInput(trimmed);
            terminal.clearInput();
            document.querySelector('.prompt').textContent = '..';
            return;
        }

        if (isMultiline) {
            multilineBuffer += '\n' + trimmed;
            terminal.writeInput(trimmed);
            terminal.clearInput();

            if (isEndOfBlock(multilineBuffer)) {
                isMultiline = false;
                document.querySelector('.prompt').textContent = '>>';
                var code = multilineBuffer;
                multilineBuffer = '';
                terminal.addToHistory(code);
                runCode(code);
            }
            return;
        }

        terminal.writeInput(trimmed);
        terminal.addToHistory(trimmed);
        terminal.clearInput();
        runCode(trimmed);
    }

    function runCode(code) {
        console.log('[REPL] Executing:', code);

        var t0 = performance.now();
        try {
            var result  = Module.repl_execute(code);
            var elapsed = performance.now() - t0;
            execTime.textContent = elapsed.toFixed(1) + 'ms';

            console.log('[REPL] Result:', JSON.stringify(result));

            if (result && result.length) {
                if (result === '__CLEAR__') {
                    terminal.clear();
                } else if (/^Error:/m.test(result)) {
                    terminal.writeError(result);
                } else if (/^Warning:/m.test(result)) {
                    terminal.writeWarning(result);
                } else {
                    terminal.writeLine(result);
                }
            }
        } catch (err) {
            console.error('[REPL] Execution error:', err);
            terminal.writeError('Runtime error: ' + err.message);
        }
    }

    // ══════════════════════════════════════
    // Multiline
    // ══════════════════════════════════════

    function needsMoreInput(code) {
        var first = code.trim().split(/\s+/)[0];
        var blocks = ['for','while','if','switch','try','function','classdef'];
        if (blocks.indexOf(first) !== -1 && !hasMatchingEnd(code)) return true;
        if (code.trimEnd().slice(-3) === '...') return true;
        return false;
    }

    function isEndOfBlock(code) { return hasMatchingEnd(code); }

    function hasMatchingEnd(code) {
        var openers = ['for','while','if','switch','try','function','classdef'];
        var depth = 0;
        var lines = code.split('\n');
        for (var i = 0; i < lines.length; i++) {
            var line = lines[i].trim();
            if (line.charAt(0) === '%') continue;
            var first = line.split(/[\s(;,]+/)[0];
            if (openers.indexOf(first) !== -1) depth++;
            if (/\bend\b/.test(line)) {
                var words = line.replace(/;.*$/, '').trim().split(/\s+/);
                if (words[words.length - 1] === 'end' || words[0] === 'end') {
                    depth--;
                }
            }
        }
        return depth <= 0;
    }

    // ══════════════════════════════════════
    // Autocomplete
    // ══════════════════════════════════════

    function handleTab(value, cursor) {
        if (!isReady || !Module) return;
        var ws = cursor - 1;
        while (ws >= 0 && /[a-zA-Z0-9_]/.test(value[ws])) ws--;
        ws++;
        var partial = value.substring(ws, cursor);
        if (!partial) return;
        try {
            var raw = Module.repl_complete(partial);
            if (raw) {
                var items = raw.split(',').filter(function(s) { return s.length > 0; });
                autocomplete.show(items, partial);
            }
        } catch (e) { console.error('[REPL] complete error', e); }
    }

    // ══════════════════════════════════════
    // Button bindings
    // ══════════════════════════════════════

    terminal.onSubmit(executeCommand);
    terminal.onTab(handleTab);

    btnClear.addEventListener('click', function() { terminal.clear(); terminal.focus(); });
    btnReset.addEventListener('click', function() {
        if (Module && Module.repl_reset) {
            terminal.writeSystem(Module.repl_reset());
        }
        terminal.focus();
    });
    btnWS.addEventListener('click', function() {
        if (Module && Module.repl_workspace) {
            terminal.writeInfo(Module.repl_workspace());
        }
        terminal.focus();
    });

    // ══════════════════════════════════════
    // Start
    // ══════════════════════════════════════

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initWasm);
    } else {
        setTimeout(initWasm, 100);
    }

})();