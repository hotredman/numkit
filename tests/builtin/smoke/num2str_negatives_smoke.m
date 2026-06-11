clear
import compat.*
% num2str column width for integer vectors with NEGATIVE values. MATLAB's
% integer field is max-abs-DIGITS + 2 (the minus sign is absorbed into the
% field, not extra). DEEP-PROBE 2026-05-31: numkit's width included the sign,
% so a negative-dominant column was one space too wide. Pinned vs MATLAB R2025b.

fprintf('[%s]  (expect "-1   10 -100")\n', num2str([-1 10 -100]));
fprintf('[%s]  (expect "-100 -200")\n',     num2str([-100 -200]));
fprintf('[%s]  (expect "5 -5")\n',          num2str([5 -5]));
fprintf('[%s]  (expect "-5  -50 -500")\n',  num2str([-5 -50 -500]));
fprintf('[%s]  (expect "100   -1")\n',      num2str([100 -1]));
fprintf('[%s]  (expect "1   22  333", positives unchanged)\n', num2str([1 22 333]));
