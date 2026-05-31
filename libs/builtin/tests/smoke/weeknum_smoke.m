clear

import compat.*

% weeknum(D [, WeekStart [, European]]): week-of-year number for serial
% date number D (element-wise, shape preserved). WeekStart 1=Sunday..7=Sat
% (default 1). European=1 applies the ISO-style ">=4 days = week 1" rule
% with the chosen WeekStart, donating a short leading partial week to the
% previous year. Implemented 2026-05-30 (was an undefined function).
% vs MATLAB R2025b.

fprintf('=== US convention (WeekStart=1) ===\n');
fprintf('2026-01-01 (Thu) -> %d (expect 1)\n',  weeknum(datenum('2026-01-01')));
fprintf('2026-01-04 (Sun) -> %d (expect 2)\n',  weeknum(datenum('2026-01-04')));
fprintf('2026-12-31       -> %d (expect 53)\n', weeknum(datenum('2026-12-31')));
fprintf('2020-02-29 (leap)-> %d (expect 9)\n',  weeknum(datenum('2020-02-29')));

fprintf('\n=== WeekStart = 2 (Monday) ===\n');
fprintf('2026-01-04 (Sun) -> %d (expect 1)\n', weeknum(datenum('2026-01-04'),2));
fprintf('2026-01-05 (Mon) -> %d (expect 2)\n', weeknum(datenum('2026-01-05'),2));

fprintf('\n=== European convention (3rd arg = 1) ===\n');
fprintf('2026-01-01 -> %d (expect 53, partial week donated)\n', weeknum(datenum('2026-01-01'),1,1));
fprintf('2026-01-04 -> %d (expect 1)\n',  weeknum(datenum('2026-01-04'),1,1));
fprintf('2026-12-31 -> %d (expect 52)\n', weeknum(datenum('2026-12-31'),1,1));
fprintf('2027-01-01 -> %d (expect 52)\n', weeknum(datenum('2027-01-01'),1,1));

fprintf('\n=== vector input (shape preserved) ===\n');
w = weeknum([datenum('2026-01-01'); datenum('2026-12-31')]);
fprintf('w = %d %d  iscolumn=%d (expect 1 53 1)\n', w(1), w(2), iscolumn(w));
