clear

import compat.*

% addtodate(D, quantity, units): add a quantity of units to serial date
% number D (scalar). Time units ('day','hour','minute','second',
% 'millisecond') are plain serial arithmetic; calendar units ('month',
% 'year') add to the month/year component, clamp the day to the new
% month's last valid day, and preserve the time-of-day. Implemented
% 2026-05-30 (was an undefined function). vs MATLAB R2025b.

base = datenum(2026,1,31,10,20,30);

fprintf('=== time units (+3) ===\n');
fprintf('day    -> %s (expect 2026 2 3 10 20 30)\n',  mat2str(datevec(addtodate(base,3,'day'))));
fprintf('hour   -> %s (expect 2026 1 31 13 20 30)\n', mat2str(datevec(addtodate(base,3,'hour'))));
fprintf('minute -> %s (expect 2026 1 31 10 23 30)\n', mat2str(datevec(addtodate(base,3,'minute'))));
fprintf('second -> %s (expect 2026 1 31 10 20 33)\n', mat2str(datevec(addtodate(base,3,'second'))));

fprintf('\n=== calendar units (clamp + preserve time) ===\n');
fprintf('+3 month -> %s (expect 2026 4 30 10 20 30, Apr clamps 31->30)\n', mat2str(datevec(addtodate(base,3,'month'))));
fprintf('+3 year  -> %s (expect 2029 1 31 10 20 30)\n', mat2str(datevec(addtodate(base,3,'year'))));

fprintf('\n=== day-clamping edges ===\n');
v = datevec(addtodate(datenum(2026,1,31),1,'month'));
fprintf('Jan31 +1mo       -> %d-%d-%d (expect 2026-2-28)\n', v(1),v(2),v(3));
v = datevec(addtodate(datenum(2024,1,31),1,'month'));
fprintf('Jan31 2024 +1mo  -> %d-%d-%d (expect 2024-2-29, leap)\n', v(1),v(2),v(3));
v = datevec(addtodate(datenum(2024,2,29),1,'year'));
fprintf('Feb29 2024 +1yr  -> %d-%d-%d (expect 2025-2-28)\n', v(1),v(2),v(3));
v = datevec(addtodate(datenum(2026,3,15),-4,'month'));
fprintf('Mar15 2026 -4mo  -> %d-%d-%d (expect 2025-11-15)\n', v(1),v(2),v(3));
