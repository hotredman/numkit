clear
import compat.*

fprintf('=== now (MATLAB serial date number) ===\n');
fprintf('  now = %.6f (current time as days since year 0000-01-00)\n', now);
fprintf('  Expected range for 2026: ~740300\n');
fprintf('  1970-01-01 = 719529 (Unix epoch)\n');
fprintf('  Today - 1970 = %g days = %.1f years\n', now - 719529, (now - 719529) / 365.25);
