clear
import compat.*

% strcmp / strcmpi / strncmp / strncmpi compare element-wise with broadcast
% on string-array operands (MATLAB R2025b): a char/string scalar broadcasts
% against an array, two equal-size arrays compare element-wise, and a string
% array vs a cell mixes. Two scalars -> a logical scalar (unchanged).

% ── string array vs scalar -> logical array ─────────────────────
a = strcmp(["a" "b" "a"], "a");
fprintf('strcmp arr-vs-scalar  islogical=%d numel=%d v=[%d %d %d] (expect 1, 3, 1 0 1)\n', ...
        islogical(a), numel(a), a(1), a(2), a(3));

% ── two equal-size string arrays ────────────────────────────────
b = strcmp(["ab" "cd"], ["ab" "xy"]);
fprintf('strcmp arr-vs-arr  v=[%d %d] (expect 1 0)\n', b(1), b(2));

% ── strcmpi / strncmp / strncmpi ────────────────────────────────
e = strcmpi(["A" "b"], "a");
fprintf('strcmpi  v=[%d %d] (expect 1 0)\n', e(1), e(2));
f = strncmp(["abc" "abd" "xyz"], "ab", 2);
fprintf('strncmp  v=[%d %d %d] (expect 1 1 0)\n', f(1), f(2), f(3));
fi = strncmpi(["ABc" "xyz"], "ab", 2);
fprintf('strncmpi  v=[%d %d] (expect 1 0)\n', fi(1), fi(2));

% ── string array vs cell mixes ──────────────────────────────────
g = strcmp(["a" "b"], {'a' 'x'});
fprintf('strcmp str-vs-cell  v=[%d %d] (expect 1 0)\n', g(1), g(2));

% ── column array shape preserved ────────────────────────────────
col = strcmp(["a"; "b"], "a");
fprintf('strcmp col  sz=%dx%d (expect 2x1)\n', size(col,1), size(col,2));

% ── unchanged: scalar + cell paths ──────────────────────────────
fprintf('strcmp scalar  v=%d (expect 1)\n', strcmp("ab", "ab"));
c = strcmp({'a','b'}, {'a','x'});
fprintf('strcmp cell-vs-cell  v=[%d %d] (expect 1 0)\n', c(1), c(2));
