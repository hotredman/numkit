clear
import compat.*

% strfind on a cell / string-array source returns a SAME-SHAPE cell of
% 1-based index vectors (MATLAB R2025b). A scalar char / string source
% still returns a plain double row vector.

% ── string array -> same-shape cell of index vectors ────────────
sf = strfind(["hello" "world" "lll"], "l");
fprintf('strfind str-arr  iscell=%d numel=%d e1=[%s] e2=[%s] e3=[%s]\n', ...
        iscell(sf), numel(sf), num2str(sf{1}), num2str(sf{2}), num2str(sf{3}));
fprintf('  (expect 1, 3, [3 4], [4], [1 2 3])\n');

% ── cell source (previously threw "Not a char array") ───────────
sc = strfind({'hello','xx'}, 'l');
fprintf('strfind cell  iscell=%d e1=[%s] e2empty=%d (expect 1, [3 4], 1)\n', ...
        iscell(sc), num2str(sc{1}), isempty(sc{2}));

% ── scalar char / string -> double row vector (unchanged) ───────
ss = strfind("banana", "an");
fprintf('strfind str-scalar  iscell=%d v=[%s] (expect 0, [2 4])\n', iscell(ss), num2str(ss));
ch = strfind('abcabc', 'bc');
fprintf('strfind char  v=[%s] (expect [2 5])\n', num2str(ch));

% ── column string array -> column cell ──────────────────────────
col = strfind(["ab"; "cb"], "b");
fprintf('strfind col  sz=%dx%d (expect 2x1)\n', size(col,1), size(col,2));
