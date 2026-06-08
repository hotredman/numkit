clear

import compat.*

% SOS section ordering (DEEP-PROBE 2026-05-31). MATLAB's zp2sos/tf2sos
% default 'up' ordering sorts sections by ASCENDING pole radius: poles
% nearest the ORIGIN come first, poles nearest the unit circle come last,
% and the overall gain is folded into the first (origin-nearest) section.
% numkit previously emitted sections in the reversed (unit-circle-first)
% order. vs MATLAB R2025b. (Works for complex-conjugate pole pairs — the
% normal butter/cheby/ellip case; pairing of purely real poles still uses
% numkit's own grouping, which leaves the cascade product unchanged.)

fprintf('=== zp2sos, complex poles 0.2+/-0.3i (r=0.36) and 0.5+/-0.5i (r=0.71) ===\n');
S = zp2sos([], [0.5+0.5i; 0.5-0.5i; 0.2+0.3i; 0.2-0.3i], 1);
fprintf('row1 denom = [1 %.4g %.4g]  (expect [1 -0.4 0.13], origin-nearest first)\n', S(1,5), S(1,6));
fprintf('row2 denom = [1 %.4g %.4g]  (expect [1 -1 0.5], unit-circle-nearest last)\n', S(2,5), S(2,6));

fprintf('\n=== tf2sos, 4th-order [1 2 3 4 5]/[1 0.5 0.3 0.1 0.05] ===\n');
sm = tf2sos([1 2 3 4 5], [1 0.5 0.3 0.1 0.05]);
fprintf('row1 = [%.6g %.6g %.6g | 1 %.6g %.6g]\n', sm(1,1), sm(1,2), sm(1,3), sm(1,5), sm(1,6));
fprintf('       (expect [1 -0.575631 2.088157 | 1 -0.208778 0.210917])\n');
fprintf('row2 = [%.6g %.6g %.6g | 1 %.6g %.6g]\n', sm(2,1), sm(2,2), sm(2,3), sm(2,5), sm(2,6));
fprintf('       (expect [1 2.575631 2.394456 | 1 0.708778 0.237061])\n');
fprintf('ascending radius (a2 row1 < a2 row2): %d  (expect 1)\n', sm(1,6) < sm(2,6));

fprintf('\n=== 6th-order Butterworth tf2sos: 3 sections, gain on first ===\n');
b = [0.00258506418423728 0.0155103851054237 0.0387759627635592 0.0517012836847456 0.0387759627635592 0.0155103851054237 0.00258506418423728];
a = [1 -2.37972104455478 2.91040656786469 -2.0551314367731 0.87792389763409 -0.20986545035969 0.0218315739799719];
s3 = tf2sos(b, a);
fprintf('a2 per row = [%.6g %.6g %.6g]  (expect ascending 0.122681 0.272215 0.653728)\n', s3(1,6), s3(2,6), s3(3,6));
fprintf('row1 b0 = %.6g  (gain folded into origin-nearest section, expect ~0.002585)\n', s3(1,1));
