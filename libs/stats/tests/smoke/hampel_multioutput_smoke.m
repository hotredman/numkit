clear

import compat.*

% hampel multi-output [y, i, xmedian, xsigma] — DEEP-PROBE 2026-05-31.
% Only the filtered signal y was implemented; the documented outlier
% mask (i), local median (xmedian), and local sigma (xsigma = 1.4826*MAD)
% outputs were missing. The MAD->sigma factor was also the loose 1.4826
% rather than the MATLAB-exact 1/norminv(0.75) = 1.482602218505602.
% Reference: MATLAB R2025b.

x = [1 2 100 3 4];
[y, i, xmed, xsig] = hampel(x);

fprintf('=== hampel([1 2 100 3 4]) default k=3, nsigma=3 ===\n');
fprintf('y    = %g %g %g %g %g   (expect 1 2 3 3 4)\n', y(1),y(2),y(3),y(4),y(5));
fprintf('i    = %g %g %g %g %g   (expect 0 0 1 0 0, logical)\n', i(1),i(2),i(3),i(4),i(5));
fprintf('xmed = %g %g %g %g %g   (expect 2.5 3 3 3 3.5)\n', xmed(1),xmed(2),xmed(3),xmed(4),xmed(5));
fprintf('xsig(1) = %.12f   (expect 1.482602218506)\n', xsig(1));

fprintf('\n=== k=2, two outliers [1 2 3 100 5 6 7 200 9 10] ===\n');
[y2, i2, m2, s2] = hampel([1 2 3 100 5 6 7 200 9 10], 2);
fprintf('outliers flagged = %g  (expect 2)\n', sum(double(i2)));
fprintf('y2(4)=%g y2(8)=%g  (expect 5 and 9)\n', y2(4), y2(8));
fprintf('s2(4) = %.12f   (expect 2.965204437011)\n', s2(4));

fprintf('\n=== 1-output form unchanged ===\n');
z = hampel(x);
fprintf('z = %g %g %g %g %g   (expect 1 2 3 3 4)\n', z(1),z(2),z(3),z(4),z(5));
