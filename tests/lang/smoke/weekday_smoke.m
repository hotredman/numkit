clear
import compat.*

fprintf('=== weekday (day-of-week 1=Sun..7=Sat) ===\n');

% Single date
[d, n] = weekday(datenum(2026, 5, 9));
fprintf('  weekday(2026-05-09) = %d / "%s"  (expect 7 / "Sat")\n', d, n);

[d, n] = weekday(datenum(2026, 5, 10));
fprintf('  weekday(2026-05-10) = %d / "%s"  (expect 1 / "Sun")\n', d, n);

% Long name
[d, n] = weekday(datenum(2026, 5, 9), 'long');
fprintf('  weekday(2026-05-09, ''long'') = %d / "%s"  (expect 7 / "Saturday")\n', d, n);

% Vector input
v = weekday([datenum(2026,5,9); datenum(2026,5,10); datenum(2026,5,11)]);
fprintf('  vec input: [%d %d %d]  (expect [7 1 2])\n', v(1), v(2), v(3));

% Historical date — 1970-01-01 was Thursday
fprintf('  weekday(1970-01-01) = %d  (expect 5 = Thu)\n', weekday(datenum(1970,1,1)));

% Reference: 2000-01-01 was Saturday
fprintf('  weekday(2000-01-01) = %d  (expect 7 = Sat)\n', weekday(datenum(2000,1,1)));

% Reference: 1999-12-31 was Friday
fprintf('  weekday(1999-12-31) = %d  (expect 6 = Fri)\n', weekday(datenum(1999,12,31)));
