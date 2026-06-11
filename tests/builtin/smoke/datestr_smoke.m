clear

import compat.*

% datestr: format a serial date number / 1x6 date vector as text.
% Implemented 2026-05-30 (was an undefined function). vs MATLAB R2025b.
% 738885.5 = 30-Dec-2022 12:00:00 (a Friday).

fprintf('=== default format (auto date vs date+time) ===\n');
fprintf('datestr(738885)   = [%s] (expect 30-Dec-2022)\n', datestr(738885));
fprintf('datestr(738885.5) = [%s] (expect 30-Dec-2022 12:00:00)\n', datestr(738885.5));

fprintf('\n=== format-string tokens ===\n');
fprintf('yyyy-mm-dd        = [%s] (expect 2022-12-30)\n', datestr(738885.5, 'yyyy-mm-dd'));
fprintf('yyyy/mm/dd HH:MM:SS = [%s] (expect 2022/12/30 12:33:07)\n', datestr(738885.523, 'yyyy/mm/dd HH:MM:SS'));
fprintf('mmmm dd, yyyy     = [%s] (expect December 30, 2022)\n', datestr(738885, 'mmmm dd, yyyy'));
fprintf('ddd               = [%s] (expect Fri)\n', datestr(738885, 'ddd'));
fprintf('dddd              = [%s] (expect Friday)\n', datestr(738885, 'dddd'));
fprintf('yy                = [%s] (expect 22)\n', datestr(738885, 'yy'));

fprintf('\n=== 1x6 date-vector input ===\n');
fprintf('[2022 12 30 6 5 9] = [%s] (expect 2022-12-30 06:05:09)\n', ...
        datestr([2022 12 30 6 5 9], 'yyyy-mm-dd HH:MM:SS'));
