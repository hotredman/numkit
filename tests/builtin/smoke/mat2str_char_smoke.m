clear
import compat.*
% mat2str on CHAR input. DEEP-PROBE 2026-05-31: char previously rendered
% numeric codes (mat2str('abc') -> '[97 98 99]'). MATLAB renders a quoted
% char literal; a char matrix becomes ['ab';'cd']; internal quotes double.

fprintf('row:        [%s]  (expect ''abc'')\n', mat2str('abc'));
fprintf('matrix:     [%s]  (expect [''ab'';''cd''])\n', mat2str(['ab';'cd']));
fprintf('empty:      [%s]  (expect '''''')\n', mat2str(''));
fprintf('quote:      [%s]  (expect ''a''''b'')\n', mat2str('a''b'));
fprintf('single ch:  [%s]  (expect ''x'')\n', mat2str('x'));

% Unchanged: numeric / logical / complex.
fprintf('double mat: [%s]  (expect [1 2;3 4])\n', mat2str([1 2;3 4]));
fprintf('logical:    [%s]  (expect [true false])\n', mat2str([true false]));
fprintf('complex:    [%s]  (expect [1+2i 3-4i])\n', mat2str([1+2i 3-4i]));
