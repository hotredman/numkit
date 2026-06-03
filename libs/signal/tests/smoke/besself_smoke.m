clear
import compat.*

% besself — analog Bessel filter. DEEP-PROBE 2026-06: the default (no 's')
% ran a bogus digital path and returned binomial (s+Wo)^n coefficients;
% besself is ALWAYS analog in MATLAB. Reference: MATLAB R2025b.

[b3, a3] = besself(3, 1);
fprintf('besself(3,1) a: '); disp(a3);   % [1 2.432881 2.466212 1]
fprintf('besself(3,1) b: '); disp(b3);   % [0 0 0 1]

[b2, a2] = besself(2, 1);
fprintf('besself(2,1) a: '); disp(a2);   % [1 1.732051 1]

[b4, a4] = besself(4, 2);
fprintf('besself(4,2) a: '); disp(a4);   % [1 6.24788 17.5662 25.6087 16]

[bh, ah] = besself(3, 2, 'high');
fprintf('besself(3,2,high) a: '); disp(ah);  % [1 4.932424 9.731523 8]

% 's' is redundant (same as default).
fprintf('default equals s-flag: %d  (expect 1)\n', ...
        isequal(besself(3,1), besself(3,1,'s')));
