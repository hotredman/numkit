(function () {
    'use strict';

    const outputEl   = document.getElementById('terminal-output');
    const inputEl    = document.getElementById('terminal-input');
    const dropdownEl = document.getElementById('autocomplete-dropdown');
    const statusText = document.getElementById('status-text');
    const execTime   = document.getElementById('exec-time');
    const btnClear   = document.getElementById('btn-clear');
    const btnReset   = document.getElementById('btn-reset');
    const btnWS      = document.getElementById('btn-workspace');

    const terminal     = new Terminal(outputEl, inputEl);
    const autocomplete = new AutocompleteManager(dropdownEl, inputEl);

    let Module  = null;
    let isReady = false;

    let multilineBuffer = '';
    let isMultiline     = false;

    // ── Init ──

    async function initWasm() {
        statusText.textContent = 'Loading WebAssembly…';
        console.log('[REPL] Starting WASM init...');

        try {
            // Проверяем что фабричная функция существует
            if (typeof createMatlabModule === 'undefined') {
                throw new Error('createMatlabModule not found. Check that matlab_repl.js is loaded.');
            }

            console.log('[REPL] createMatlabModule found, calling...');

            // Вызываем фабрику Emscripten
            // Передаём locateFile чтобы .wasm нашёлся рядом с .js
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

            console.log('[REPL] Module loaded:', Module);
            console.log('[REPL] Available functions:',
                Object.keys(Module).filter(k => k.startsWith('repl_')));

            // Проверяем что наши функции доступны
            if (typeof Module.repl_init !== 'function') {
                throw new Error('repl_init not found in module. Embind may have failed.');
            }

            const msg = Module.repl_init();
            console.log('[REPL] repl_init returned:', msg);
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
            terminal.writeError('Check browser console (F12) for details.');
            initFallback();
        }
    }

    function initFallback() {
        terminal.writeSystem('\nRunning in FALLBACK mode (no WASM).');
        terminal.writeSystem('Build: emcmake cmake .. -DMLAB_BUILD_REPL=ON && emmake make\n');

        Module = {
            repl_execute(c) {
                if (c === 'clear') return 'Workspace cleared.';
                return '[fallback] ' + c;
            },
            repl_complete(p) {
                const kw = ['disp','fprintf','for','while','if','end',
                    'function','clear','clc','who','zeros','ones',
                    'eye','rand','sin','cos','sqrt','plot','help'];
                return kw.filter(k => k.startsWith(p)).join(',');
            },
            repl_reset()     { return 'Workspace cleared.'; },
            repl_workspace() { return 'No variables.'; },
        };

        isReady = true;
        statusText.textContent = 'Fallback mode';
        statusText.className  = 'ready';
        terminal.focus();
    }

    // ── Execute ──

    function executeCommand(input) {
        const trimmed = input.trim();
        if (!trimmed) { terminal.clearInput(); return; }

        if (!isReady || !Module) {
            terminal.writeError('Interpreter not ready yet.');
            terminal.clearInput();
            return;
        }

        // Multiline: начало блока
        if (needsMoreInput(trimmed) && !isMultiline) {
            isMultiline = true;
            multilineBuffer = trimmed;
            terminal.writeInput(trimmed);
            terminal.clearInput();
            document.querySelector('.prompt').textContent = '..';
            return;
        }

        // Multiline: продолжение
        if (isMultiline) {
            multilineBuffer += '\n' + trimmed;
            terminal.writeInput(trimmed);
            terminal.clearInput();

            if (isEndOfBlock(multilineBuffer)) {
                isMultiline = false;
                document.querySelector('.prompt').textContent = '>>';
                const code = multilineBuffer;
                multilineBuffer = '';
                terminal.addToHistory(code);
                runCode(code);
            }
            return;
        }

        // Однострочная команда
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

    // ── Multiline ──

    function needsMoreInput(code) {
        const first = code.trim().split(/\s+/)[0];
        const blocks = ['for','while','if','switch','try','function','classdef'];
        if (blocks.includes(first) && !hasMatchingEnd(code)) return true;
        if (code.trimEnd().endsWith('...')) return true;
        return false;
    }

    function isEndOfBlock(code) { return hasMatchingEnd(code); }

    function hasMatchingEnd(code) {
        const openers = ['for','while','if','switch','try','function','classdef'];
        let depth = 0;
        for (const raw of code.split('\n')) {
            const line = raw.trim();
            if (line.startsWith('%')) continue;
            const first = line.split(/[\s(;,]+/)[0];
            if (openers.includes(first)) depth++;
            // "end" может быть с ; или пробелами после
            if (/\bend\b/.test(line)) {
                // Считаем только если end — отдельное слово в конце
                const words = line.replace(/;.*$/, '').trim().split(/\s+/);
                if (words[words.length - 1] === 'end' || words[0] === 'end') {
                    depth--;
                }
            }
        }
        return depth <= 0;
    }

    // ── Autocomplete ──

    function handleTab(value, cursor) {
        if (!isReady || !Module) return;
        let ws = cursor - 1;
        while (ws >= 0 && /[a-zA-Z0-9_]/.test(value[ws])) ws--;
        ws++;
        const partial = value.substring(ws, cursor);
        if (!partial) return;
        try {
            const raw = Module.repl_complete(partial);
            if (raw) {
                const items = raw.split(',').filter(Boolean);
                autocomplete.show(items, partial);
            }
        } catch (e) { console.error('[REPL] complete error', e); }
    }

    // ── Buttons ──

    terminal.onSubmit(executeCommand);
    terminal.onTab(handleTab);

    btnClear.addEventListener('click', () => { terminal.clear(); terminal.focus(); });
    btnReset.addEventListener('click', () => {
        if (Module && Module.repl_reset) {
            terminal.writeSystem(Module.repl_reset());
        }
        terminal.focus();
    });
    btnWS.addEventListener('click', () => {
        if (Module && Module.repl_workspace) {
            terminal.writeInfo(Module.repl_workspace());
        }
        terminal.focus();
    });

    // ── Start! ──

    // Дожидаемся полной загрузки страницы
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initWasm);
    } else {
        // Небольшая задержка чтобы matlab_repl.js точно инициализировался
        setTimeout(initWasm, 100);
    }

})();