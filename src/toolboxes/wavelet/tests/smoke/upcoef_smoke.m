clear
import compat.*

% upcoef: direct single-branch reconstruction up N levels.
% bugs/wavelet/upcoef. Per level: interleave zeros + conv with Lo_R
% (or Hi_R on the first level for a detail branch).

y = upcoef('a', 5, 'db1', 2);
fprintf('a 5 db1 2 = [%s]  (expect [2.5 2.5 2.5 2.5])\n', num2str(y, '%.4f '));

y = upcoef('d', 5, 'db1', 2);
fprintf('d 5 db1 2 = [%s]  (expect [2.5 2.5 -2.5 -2.5])\n', num2str(y, '%.4f '));

y = upcoef('a', [1 2], 'db2', 1);
fprintf('a [1 2] db2 1 = [%s]\n', num2str(y, '%.6f '));
fprintf('  (expect [0.482963 0.836516 1.190074 1.543628 0.448288 -0.258819])\n');

y = upcoef('a', [1 2 3], 'db1', 1);
fprintf('a [1 2 3] db1 1 = [%s]  (expect [0.7071 0.7071 1.4142 1.4142 2.1213 2.1213])\n', ...
        num2str(y, '%.4f '));
