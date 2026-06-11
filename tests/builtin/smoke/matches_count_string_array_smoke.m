clear
import compat.*

% matches / count return a SAME-SHAPE array for a string-array source
% (MATLAB R2025b): matches -> logical array, count -> double array. A
% scalar string -> scalar; a cell -> same-shape array (unchanged).

% ── matches: string array -> logical array ──────────────────────
m = matches(["cat" "dog" "cat"], "cat");
fprintf('matches  islogical=%d numel=%d v=[%d %d %d] (expect 1, 3, 1 0 1)\n', ...
        islogical(m), numel(m), m(1), m(2), m(3));

% multi-pattern: match if equal to ANY alternative
mc = matches(["cat" "dog" "fish"], ["cat" "fish"]);
fprintf('matches multi-pat  v=[%d %d %d] (expect 1 0 1)\n', mc(1), mc(2), mc(3));

% scalar + cell unchanged
fprintf('matches scalar  v=%d (expect 1)\n', matches("cat", "cat"));
mcl = matches({'a','b'}, 'a');
fprintf('matches cell  v=[%d %d] (expect 1 0)\n', mcl(1), mcl(2));

% ── count: string array -> double array ─────────────────────────
c = count(["aa" "aba"], "a");
fprintf('count  numel=%d v=[%d %d] (expect 2, 2 2)\n', numel(c), c(1), c(2));
fprintf('count scalar  v=%d (expect 3)\n', count("banana", "a"));
ccl = count({'aa','aaa'}, 'a');
fprintf('count cell  v=[%d %d] (expect 2 3)\n', ccl(1), ccl(2));

% ── column string array: shape preserved ────────────────────────
mcol = matches(["a"; "b"], "a");
fprintf('matches col  sz=%dx%d (expect 2x1)\n', size(mcol,1), size(mcol,2));
