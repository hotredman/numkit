clear

import compat.*

% sprintf/fprintf %s width + precision. Bug fixed 2026-05-30: %s ignored
% its width/precision spec and printed the raw string. Now it honours them
% like MATLAB/C printf. vs MATLAB R2025b.

fprintf('=== width (right / left justify) ===\n');
fprintf('[%5s]  (expect [   hi])\n', 'hi');
fprintf('[%-5s]  (expect [hi   ])\n', 'hi');

fprintf('\n=== precision (char cap) ===\n');
fprintf('[%.1s]  (expect [h])\n', 'hi');
fprintf('[%.3s]  (expect [hel])\n', 'hello');

fprintf('\n=== width + precision combined ===\n');
fprintf('[%5.1s]  (expect [    h])\n', 'hello');

fprintf('\n=== applies to the string type too ===\n');
fprintf('[%5s]  (expect [   hi])\n', "hi");

fprintf('\n=== plain %%s still works (regression) ===\n');
fprintf('[%s]  (expect [hello])\n', 'hello');
