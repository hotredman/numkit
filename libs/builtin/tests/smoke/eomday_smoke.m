clear
import compat.*

fprintf('=== eomday (last day of given month) ===\n');

% Scalar
fprintf('  eomday(2026, 1)  = %d  (expect 31)\n', eomday(2026, 1));
fprintf('  eomday(2026, 2)  = %d  (expect 28, common year)\n', eomday(2026, 2));
fprintf('  eomday(2024, 2)  = %d  (expect 29, leap /4)\n', eomday(2024, 2));
fprintf('  eomday(2000, 2)  = %d  (expect 29, leap /400)\n', eomday(2000, 2));
fprintf('  eomday(1900, 2)  = %d  (expect 28, century not /400)\n', eomday(1900, 2));
fprintf('  eomday(2026, 4)  = %d  (expect 30, April)\n', eomday(2026, 4));
fprintf('  eomday(2026, 12) = %d  (expect 31, December)\n', eomday(2026, 12));

% Vector form (same length)
v = eomday([2024; 2025; 2026], [2; 2; 2]);
fprintf('  vec: [%d %d %d]  (expect [29 28 28])\n', v(1), v(2), v(3));

% Scalar y, vector m -- broadcast
v2 = eomday(2024, 1:12);
fprintf('  eomday(2024, 1:12) = ');
fprintf('%d ', v2);
fprintf('\n    expect 31 29 31 30 31 30 31 31 30 31 30 31\n');

% Check shape preservation: 2x2 input
M = eomday([2024 2025; 2026 2027], [1 2; 3 4]);
fprintf('  matrix shape: M(1,1)=%d M(1,2)=%d M(2,1)=%d M(2,2)=%d\n', ...
        M(1,1), M(1,2), M(2,1), M(2,2));
fprintf('    expect 31 28 31 30\n');
