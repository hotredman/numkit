clear
import compat.*

fprintf('=== juliandate (Julian day number from date components) ===\n');

% Well-known epoch anchors
fprintf('  juliandate(1970,1,1,0,0,0)   = %.4f  (expect 2440587.5 = Unix epoch)\n', ...
        juliandate(1970, 1, 1, 0, 0, 0));
fprintf('  juliandate(2000,1,1,12,0,0)  = %.4f  (expect 2451545.0 = J2000.0)\n', ...
        juliandate(2000, 1, 1, 12, 0, 0));

% 3-arg form (00:00 default)
fprintf('  juliandate(2026,5,9)         = %.4f  (expect 2461169.5)\n', ...
        juliandate(2026, 5, 9));

% 6-arg form (noon)
fprintf('  juliandate(2026,5,9,12,0,0)  = %.4f  (expect 2461170.0)\n', ...
        juliandate(2026, 5, 9, 12, 0, 0));

% Single-arg matrix Nx3
M = [2026 5 9; 2027 5 9; 2028 5 9];
jv = juliandate(M);
fprintf('  juliandate(Nx3): [%.1f %.1f %.1f]\n', jv(1), jv(2), jv(3));
fprintf('    expect [2461169.5 2461534.5 2461900.5]\n');

% Vectorised 3-arg form
jvv = juliandate([2026; 2027; 2028], [1; 1; 1], [1; 1; 1]);
fprintf('  vec form: [%.1f %.1f %.1f]\n', jvv(1), jvv(2), jvv(3));
fprintf('    expect [2461041.5 2461406.5 2461771.5]\n');

% Single-arg row 1x6
fprintf('  juliandate([2026 5 9 12 0 0]) = %.4f  (expect 2461170.0)\n', ...
        juliandate([2026 5 9 12 0 0]));
