clear

import compat.*

% datenum(str [, fmt]): parse a date string (inverse of datestr). Bug fixed
% 2026-05-30: datenum threw "string parsing not yet supported" on a string.
% vs MATLAB R2025b. 738885 = 30-Dec-2022.

fprintf('=== auto-detected formats ===\n');
fprintf('2022-12-30          -> %.5f (expect 738885.00000)\n', datenum('2022-12-30'));
fprintf('30-Dec-2022         -> %.5f (expect 738885.00000)\n', datenum('30-Dec-2022'));
fprintf('2022-12-30 12:34:56 -> %.5f (expect 738885.52426)\n', datenum('2022-12-30 12:34:56'));
fprintf('30-Dec-2022 06:05:09-> %.5f (expect 738885.25358)\n', datenum('30-Dec-2022 06:05:09'));

fprintf('\n=== explicit format string ===\n');
fprintf('2022-12-30 yyyy-mm-dd -> %.5f (expect 738885)\n', datenum('2022-12-30', 'yyyy-mm-dd'));
fprintf('30/12/2022 dd/mm/yyyy -> %.5f (expect 738885)\n', datenum('30/12/2022', 'dd/mm/yyyy'));

fprintf('\n=== round-trips against datestr ===\n');
s = datestr(738885.5, 'yyyy-mm-dd HH:MM:SS');
fprintf('datestr->datenum -> %.5f (expect 738885.50000)\n', datenum(s, 'yyyy-mm-dd HH:MM:SS'));

fprintf('\n=== numeric form unchanged ===\n');
fprintf('datenum(2022,12,30) -> %.1f (expect 738885)\n', datenum(2022, 12, 30));
