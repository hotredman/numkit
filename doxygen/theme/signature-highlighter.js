// doxygen/theme/signature-highlighter.js
// Enhances C++ member function prototypes with crisp, modern syntax highlighting.

(function() {
    const CXX_KEYWORDS = new Set([
        'const', 'volatile', 'static', 'constexpr', 'inline', 'virtual', 'explicit',
        'noexcept', 'override', 'final', 'mutable', 'register', 'template', 'typename',
        'struct', 'class', 'enum', 'auto'
    ]);

    const CXX_PRIMITIVE_TYPES = new Set([
        'void', 'bool', 'char', 'short', 'int', 'long', 'float', 'double',
        'int8_t', 'int16_t', 'int32_t', 'int64_t',
        'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
        'size_t', 'ssize_t', 'intptr_t', 'uintptr_t', 'ptrdiff_t',
        'Value', 'Span', 'Dims', 'ValueType', 'FnHandle',
        'std::string', 'std::string_view', 'std::vector', 'std::array', 'std::tuple', 'std::pair',
        'std::pmr::memory_resource', 'std::pmr::polymorphic_allocator',
        'PolarPair', 'CylTriple', 'CartPair', 'CartTriple', 'SphTriple',
        'PolyDiv', 'PadeCoef', 'ResidueResult', 'EllipKE', 'EllipJ',
        'AudioData', 'AudioInfo', 'NormalizeResult', 'RobustfitResult', 'RobustcovResult', 'DeconvregResult',
        'Tf2ZpResult', 'BodeResult', 'NyquistResult', 'CovarResult', 'LassoResult', 'WenergyResult'
    ]);

    function formatTextTokens(text, container) {
        const tokens = text.split(/(\b[a-zA-Z_][a-zA-Z0-9_]*(?:::[a-zA-Z0-9_]+)*\b|[&*\(\)<>,=]|"[^"]*")/g);
        tokens.forEach(tok => {
            if (!tok) return;
            const trimmed = tok.trim();
            if (CXX_KEYWORDS.has(trimmed)) {
                const span = document.createElement('span');
                span.className = 'cxx-keyword';
                span.textContent = tok;
                container.appendChild(span);
            } else if (CXX_PRIMITIVE_TYPES.has(trimmed) || /^[A-Z][a-zA-Z0-9_]*$/.test(trimmed)) {
                const span = document.createElement('span');
                span.className = 'cxx-type';
                span.textContent = tok;
                container.appendChild(span);
            } else if (tok === '&' || tok === '*' || tok === '<' || tok === '>') {
                const span = document.createElement('span');
                span.className = 'cxx-symbol';
                span.textContent = tok;
                container.appendChild(span);
            } else {
                container.appendChild(document.createTextNode(tok));
            }
        });
    }

    function highlightParamType(cell) {
        if (!cell || cell.dataset.highlighted) return;
        cell.dataset.highlighted = 'true';

        const nodes = Array.from(cell.childNodes);
        cell.innerHTML = '';

        nodes.forEach(node => {
            if (node.nodeType === Node.TEXT_NODE) {
                formatTextTokens(node.textContent, cell);
            } else if (node.nodeType === Node.ELEMENT_NODE) {
                if (node.tagName === 'A' || node.classList.contains('el')) {
                    node.classList.add('cxx-type-link');
                }
                cell.appendChild(node);
            }
        });
    }

    function highlightMemName(cell) {
        if (!cell || cell.dataset.highlighted) return;
        cell.dataset.highlighted = 'true';

        // Example: "Value numkit::wavelet::wextend" or "const Value & numkit::foo"
        const fullText = cell.textContent.trim();
        if (!fullText) return;

        // Split return type and function identifier (the last token)
        const match = fullText.match(/^(.*?)([\w:~]+)$/);
        if (match) {
            const returnTypeRaw = match[1].trim();
            const funcNameRaw = match[2].trim();

            cell.innerHTML = '';

            if (returnTypeRaw) {
                const retSpan = document.createElement('span');
                retSpan.className = 'cxx-return-type';
                formatTextTokens(returnTypeRaw, retSpan);
                cell.appendChild(retSpan);
                cell.appendChild(document.createTextNode(' '));
            }

            const nameSpan = document.createElement('span');
            nameSpan.className = 'cxx-func-name';
            nameSpan.textContent = funcNameRaw;
            cell.appendChild(nameSpan);
        }
    }

    function highlightMemProto() {
        document.querySelectorAll('.memproto table.memname').forEach(table => {
            const memNameCell = table.querySelector('td.memname');
            if (memNameCell) {
                highlightMemName(memNameCell);
            }

            table.querySelectorAll('td.paramtype').forEach(cell => {
                highlightParamType(cell);
            });

            table.querySelectorAll('td.paramname').forEach(cell => {
                if (!cell.dataset.highlighted) {
                    cell.dataset.highlighted = 'true';
                    cell.classList.add('cxx-param-name');
                }
            });

            table.querySelectorAll('.paramdefval').forEach(cell => {
                if (!cell.dataset.highlighted) {
                    cell.dataset.highlighted = 'true';
                    cell.classList.add('cxx-param-defval');
                }
            });
        });
    }

    // Run on load and after any dynamic DOM update
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', highlightMemProto);
    } else {
        highlightMemProto();
    }
    
    // Also re-run after Doxygen treeview/tab navigations if needed
    window.addEventListener('load', highlightMemProto);
})();
