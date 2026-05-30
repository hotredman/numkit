clear

import compat.*

% num2str(X, FMT) format-string handling. Bug fixed 2026-05-30: num2str
% passed the double straight to snprintf, so %d-family specs printed garbage
% (num2str(5,'%05d') -> '00000') and the width padding was never trimmed.
% Now routes through the sprintf engine + strtrim. vs MATLAB R2025b.

fprintf('=== float specs (leading pad trimmed) ===\n');
fprintf('%%8.4f  pi -> [%s] (expect 3.1416)\n', num2str(pi, '%8.4f'));
fprintf('%%10.4f -pi-> [%s] (expect -3.1416)\n', num2str(-pi, '%10.4f'));

fprintf('\n=== integer specs (%%d now works) ===\n');
fprintf('%%05d 5 -> [%s] (expect 00005, leading zeros kept)\n', num2str(5, '%05d'));
fprintf('%%8d 42 -> [%s] (expect 42, NOT 0)\n', num2str(42, '%8d'));

fprintf('\n=== literal prefix: leading trimmed, internal kept ===\n');
fprintf('   value=%%6.2f -> [%s] (expect value=  3.14)\n', num2str(pi, '   value=%6.2f'));

fprintf('\n=== left-justified: trailing trimmed ===\n');
fprintf('%%-8d 5 -> [%s] (expect 5)\n', num2str(5, '%-8d'));

fprintf('\n=== default + precision forms unchanged ===\n');
fprintf('default pi -> [%s] (expect 3.1416)\n', num2str(pi));
fprintf('precision  -> [%s] (expect 3.1415927)\n', num2str(pi, 8));
