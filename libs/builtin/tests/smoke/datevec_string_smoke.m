clear

import compat.*

% datevec(str [, fmt]): parse a date string into [Y M D H MI S]. Bug fixed
% 2026-05-30: datevec threw "string parsing not yet supported" on a string.
% Completes the datestr/datenum/datevec string trio. vs MATLAB R2025b.

fprintf('=== auto-detected formats ===\n');
fprintf('2022-12-30 12:34:56 -> %s (expect [2022 12 30 12 34 56])\n', mat2str(datevec('2022-12-30 12:34:56')));
fprintf('2022-12-30          -> %s (expect [2022 12 30 0 0 0])\n', mat2str(datevec('2022-12-30')));
fprintf('30-Dec-2022         -> %s (expect [2022 12 30 0 0 0])\n', mat2str(datevec('30-Dec-2022')));

fprintf('\n=== explicit format string ===\n');
fprintf('30/12/2022 dd/mm/yyyy -> %s (expect [2022 12 30 0 0 0])\n', mat2str(datevec('30/12/2022', 'dd/mm/yyyy')));

fprintf('\n=== multi-output form ===\n');
[y, mo, d] = datevec('2022-12-30');
fprintf('[y,mo,d] = %d %d %d (expect 2022 12 30)\n', y, mo, d);

fprintf('\n=== numeric form unchanged ===\n');
fprintf('datevec(datenum(2026,5,9)) -> %s (expect [2026 5 9 0 0 0])\n', mat2str(datevec(datenum(2026,5,9))));
