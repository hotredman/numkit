clear
import compat.*

fprintf('=== mjuliandate (Modified Julian Date) ===\n');

% Well-known anchors
fprintf('  mjuliandate(1858,11,17,0,0,0)  = %.6f  (expect 0.0 -- MJD epoch)\n', ...
        mjuliandate(1858, 11, 17, 0, 0, 0));
fprintf('  mjuliandate(1970,1,1,0,0,0)    = %.6f  (expect 40587.0 -- Unix epoch)\n', ...
        mjuliandate(1970, 1, 1, 0, 0, 0));
fprintf('  mjuliandate(2000,1,1,12,0,0)   = %.6f  (expect 51544.5 -- J2000.0)\n', ...
        mjuliandate(2000, 1, 1, 12, 0, 0));
fprintf('  mjuliandate(2026,5,9)          = %.6f  (expect 61169.0)\n', ...
        mjuliandate(2026, 5, 9));
fprintf('  mjuliandate(2026,5,9,12,0,0)   = %.6f  (expect 61169.5)\n', ...
        mjuliandate(2026, 5, 9, 12, 0, 0));

% Single-arg matrix
M = [2026 5 9; 2027 5 9; 2028 5 9];
mv = mjuliandate(M);
fprintf('  mjuliandate(Nx3): [%.0f %.0f %.0f]  (expect [61169 61534 61900])\n', ...
        mv(1), mv(2), mv(3));

% Vectorised
mvv = mjuliandate([2026; 2027; 2028], [1; 1; 1], [1; 1; 1]);
fprintf('  vec form: [%.0f %.0f %.0f]  (expect [61041 61406 61771])\n', ...
        mvv(1), mvv(2), mvv(3));

% Single-arg row 1x6
fprintf('  mjuliandate([2026 5 9 12 0 0]) = %.6f  (expect 61169.5)\n', ...
        mjuliandate([2026 5 9 12 0 0]));

% Identity check: MJD = JD - 2400000.5
diff_jd_mjd = juliandate(2026, 5, 9, 12, 30, 45) - mjuliandate(2026, 5, 9, 12, 30, 45);
fprintf('  JD - MJD = %.4f  (expect 2400000.5 by definition)\n', diff_jd_mjd);
