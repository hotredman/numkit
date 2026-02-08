class Terminal {
    constructor(outputEl, inputEl) {
        this.output = outputEl;
        this.input  = inputEl;
        this.history = [];
        this.historyIndex = -1;
        this.currentInput = '';
        this.maxHistory = 500;
        this.maxOutputLines = 5000;
        this.lineCount = 0;
        this._onSubmit = null;
        this._onTab = null;

        this._setupInput();
        this._loadHistory();
    }

    writeLine(text, className) {
        if (typeof className === 'undefined') className = 'result';
        if (text === '__CLEAR__') { this.clear(); return; }

        var lines = text.split('\n');
        var frag  = document.createDocumentFragment();

        for (var i = 0; i < lines.length; i++) {
            var div = document.createElement('div');
            div.className = 'output-line ' + className;
            div.textContent = lines[i];
            frag.appendChild(div);
            this.lineCount++;
        }

        this.output.appendChild(frag);
        this._trimOutput();
        this._scrollToBottom();
    }

    writeInput(text) {
        var div = document.createElement('div');
        div.className = 'output-line input-echo';

        var prompt = document.createElement('span');
        prompt.className = 'prompt-echo';
        prompt.textContent = '>> ';

        var code = document.createElement('span');
        code.textContent = text;

        div.appendChild(prompt);
        div.appendChild(code);
        this.output.appendChild(div);
        this.lineCount++;
        this._scrollToBottom();
    }

    writeError(text)   { this.writeLine(text, 'error');   }
    writeWarning(text) { this.writeLine(text, 'warning'); }
    writeInfo(text)    { this.writeLine(text, 'info');    }
    writeSystem(text)  { this.writeLine(text, 'system');  }

    clear() {
        this.output.innerHTML = '';
        this.lineCount = 0;
    }

    getValue()     { return this.input.value; }
    setValue(text)  { this.input.value = text; this._autoResize(); }
    clearInput()   { this.input.value = '';   this._autoResize(); }
    focus()        { this.input.focus(); }

    addToHistory(cmd) {
        var t = cmd.trim();
        if (!t) return;
        if (this.history.length && this.history[this.history.length - 1] === t) {
            this.historyIndex = -1;
            return;
        }
        this.history.push(t);
        if (this.history.length > this.maxHistory)
            this.history = this.history.slice(-this.maxHistory);
        this.historyIndex = -1;
        this._saveHistory();
    }

    historyUp() {
        var self = this;
        if (!this.history.length) return;
        if (this.historyIndex === -1) {
            this.currentInput = this.input.value;
            this.historyIndex = this.history.length - 1;
        } else if (this.historyIndex > 0) {
            this.historyIndex--;
        }
        this.setValue(this.history[this.historyIndex]);
        requestAnimationFrame(function() {
            self.input.selectionStart = self.input.selectionEnd = self.input.value.length;
        });
    }

    historyDown() {
        var self = this;
        if (this.historyIndex === -1) return;
        if (this.historyIndex < this.history.length - 1) {
            this.historyIndex++;
            this.setValue(this.history[this.historyIndex]);
        } else {
            this.historyIndex = -1;
            this.setValue(this.currentInput);
        }
        requestAnimationFrame(function() {
            self.input.selectionStart = self.input.selectionEnd = self.input.value.length;
        });
    }

    onSubmit(cb) { this._onSubmit = cb; }
    onTab(cb)    { this._onTab = cb; }

    _setupInput() {
        var self = this;

        this.input.addEventListener('keydown', function(e) {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                if (self._onSubmit) self._onSubmit(self.input.value);
                return;
            }
            if (e.key === 'Enter' && e.shiftKey) {
                requestAnimationFrame(function() { self._autoResize(); });
                return;
            }
            if (e.key === 'ArrowUp' && !e.shiftKey) {
                var before = self.input.value.substring(0, self.input.selectionStart);
                if (before.indexOf('\n') === -1) { e.preventDefault(); self.historyUp(); }
                return;
            }
            if (e.key === 'ArrowDown' && !e.shiftKey) {
                var after = self.input.value.substring(self.input.selectionEnd);
                if (after.indexOf('\n') === -1) { e.preventDefault(); self.historyDown(); }
                return;
            }
            if (e.key === 'Tab') {
                e.preventDefault();
                if (self._onTab) self._onTab(self.input.value, self.input.selectionStart);
                return;
            }
            if (e.key === 'c' && e.ctrlKey) {
                if (self.input.selectionStart === self.input.selectionEnd) {
                    self.writeInput(self.input.value + '^C');
                    self.clearInput();
                }
                return;
            }
            if (e.key === 'l' && e.ctrlKey) {
                e.preventDefault();
                self.clear();
            }
        });

        this.input.addEventListener('input', function() { self._autoResize(); });

        document.getElementById('terminal-container').addEventListener('click', function(e) {
            if (e.target.id === 'terminal-output' || e.target.id === 'terminal-container')
                self.focus();
        });
    }

    _autoResize() {
        this.input.style.height = 'auto';
        this.input.style.height = this.input.scrollHeight + 'px';
        if (this.input.value.indexOf('\n') !== -1) {
            this.input.classList.add('multiline');
        } else {
            this.input.classList.remove('multiline');
        }
    }

    _scrollToBottom() {
        var out = this.output;
        requestAnimationFrame(function() { out.scrollTop = out.scrollHeight; });
    }

    _trimOutput() {
        while (this.lineCount > this.maxOutputLines && this.output.firstChild) {
            this.output.removeChild(this.output.firstChild);
            this.lineCount--;
        }
    }

    _saveHistory() {
        try { localStorage.setItem('mlab-history', JSON.stringify(this.history.slice(-100))); }
        catch(e) {}
    }

    _loadHistory() {
        try {
            var d = localStorage.getItem('mlab-history');
            if (d) this.history = JSON.parse(d);
        } catch(e) { this.history = []; }
    }
}

// ── Autocomplete ──

class AutocompleteManager {
    constructor(dropdown, input) {
        this.dropdown = dropdown;
        this.input    = input;
        this.items    = [];
        this.selectedIndex = -1;
        this.visible  = false;
        this.partial  = '';
        this._setup();
    }

    show(items, partial) {
        this.items   = items;
        this.partial = partial;
        this.selectedIndex = items.length ? 0 : -1;

        if (!items.length) { this.hide(); return; }

        if (items.length === 1) {
            this._apply(items[0]);
            this.hide();
            return;
        }

        this.dropdown.innerHTML = '';
        var self = this;
        for (var i = 0; i < items.length; i++) {
            (function(item, idx) {
                var div = document.createElement('div');
                div.className = 'autocomplete-item' + (idx === 0 ? ' selected' : '');
                var ml = partial.length;
                var matchSpan = document.createElement('span');
                matchSpan.className = 'match';
                matchSpan.textContent = item.substring(0, ml);
                div.appendChild(matchSpan);
                div.appendChild(document.createTextNode(item.substring(ml)));
                div.addEventListener('click', function() { self._apply(item); self.hide(); });
                self.dropdown.appendChild(div);
            })(items[i], i);
        }

        this.dropdown.classList.remove('hidden');
        this.visible = true;
    }

    hide() {
        this.dropdown.classList.add('hidden');
        this.visible = false;
        this.items = [];
        this.selectedIndex = -1;
    }

    navigate(dir) {
        if (!this.visible || !this.items.length) return false;
        var ch = this.dropdown.children;
        if (this.selectedIndex >= 0) ch[this.selectedIndex].classList.remove('selected');
        this.selectedIndex = (this.selectedIndex + dir + this.items.length) % this.items.length;
        ch[this.selectedIndex].classList.add('selected');
        ch[this.selectedIndex].scrollIntoView({ block: 'nearest' });
        return true;
    }

    selectCurrent() {
        if (!this.visible || this.selectedIndex < 0) return false;
        this._apply(this.items[this.selectedIndex]);
        this.hide();
        return true;
    }

    _apply(text) {
        var val = this.input.value;
        var cur = this.input.selectionStart;
        var ws = cur - 1;
        while (ws >= 0 && /[a-zA-Z0-9_]/.test(val[ws])) ws--;
        ws++;
        this.input.value = val.substring(0, ws) + text + val.substring(cur);
        var nc = ws + text.length;
        this.input.selectionStart = this.input.selectionEnd = nc;
        this.input.focus();
    }

    _setup() {
        var self = this;
        this.input.addEventListener('keydown', function(e) {
            if (!self.visible) return;
            if (e.key === 'ArrowDown')     { e.preventDefault(); self.navigate(1);  }
            else if (e.key === 'ArrowUp')  { e.preventDefault(); self.navigate(-1); }
            else if (e.key === 'Enter' || e.key === 'Tab') {
                if (self.selectCurrent()) e.preventDefault();
            }
            else if (e.key === 'Escape')   { self.hide(); e.preventDefault(); }
        });

        document.addEventListener('click', function(e) {
            if (!self.dropdown.contains(e.target) && e.target !== self.input) self.hide();
        });
    }
}