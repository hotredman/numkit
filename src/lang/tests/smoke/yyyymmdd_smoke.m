clear
import compat.*

fprintf('=== yyyymmdd (packed integer date Y*10000+M*100+D) ===\n');

fprintf('  yyyymmdd(datenum(2026,5,9))      = %d  (expect 20260509)\n', ...
        yyyymmdd(datenum(2026, 5, 9)));
fprintf('  yyyymmdd(datenum(2000,1,1))      = %d  (expect 20000101)\n', ...
        yyyymmdd(datenum(2000, 1, 1)));
fprintf('  yyyymmdd(datenum(1970,12,31))    = %d  (expect 19701231)\n', ...
        yyyymmdd(datenum(1970, 12, 31)));
fprintf('  yyyymmdd(datenum(1970,1,1))      = %d  (expect 19700101 = Unix epoch)\n', ...
        yyyymmdd(datenum(1970, 1, 1)));
fprintf('  yyyymmdd(datenum(0,1,1))         = %d  (expect 101 = year 0 / Jan 1)\n', ...
        yyyymmdd(datenum(0, 1, 1)));

% Vector form
v = yyyymmdd([datenum(2026,5,9); datenum(2027,6,10); datenum(2028,7,11)]);
fprintf('  vec: [%d %d %d]  (expect [20260509 20270610 20280711])\n', ...
        v(1), v(2), v(3));

% Shape preservation
M = yyyymmdd([datenum(2024,1,1) datenum(2024,2,2); ...
              datenum(2024,3,3) datenum(2024,4,4)]);
fprintf('  matrix shape: [%d %d]\n', size(M, 1), size(M, 2));
fprintf('  M(1,1)=%d M(1,2)=%d M(2,1)=%d M(2,2)=%d\n', M(1,1), M(1,2), M(2,1), M(2,2));
fprintf('    expect 20240101 20240202 20240303 20240404\n');
