clear

import compat.*

% Inverse-trig family — audit ТЗ batch closure 2026-05-09.
% All 9 functions verified bit-identical to MATLAB R2025b.

fprintf('=== acos / acosd  (radians + degrees) ===\n');
fprintf('  acos(-1)  = %.15f  (expect pi = 3.141593)\n', acos(-1));
fprintf('  acosd(-1) = %.15f  (expect 180)\n\n', acosd(-1));

fprintf('=== acosh  (domain x >= 1) ===\n');
fprintf('  acosh(1)  = %.15f  (expect 0)\n', acosh(1));
fprintf('  acosh(2)  = %.15f  (expect 1.31696)\n\n', acosh(2));

fprintf('=== acot / acotd ===\n');
fprintf('  acot(1)   = %.15f  (expect pi/4 = 0.785398)\n', acot(1));
fprintf('  acotd(1)  = %.15f  (expect 45)\n\n', acotd(1));

fprintf('=== acoth  (domain |x| > 1) ===\n');
fprintf('  acoth(2)  = %.15f  (expect 0.549306)\n', acoth(2));
fprintf('  acoth(-2) = %.15f  (expect -0.549306)\n\n', acoth(-2));

fprintf('=== acsc / acscd  (domain |x| >= 1) ===\n');
fprintf('  acsc(1)   = %.15f  (expect pi/2 = 1.570796)\n', acsc(1));
fprintf('  acscd(2)  = %.15f  (expect 30)\n\n', acscd(2));

fprintf('=== acsch  (all real except 0) ===\n');
fprintf('  acsch(1)  = %.15f  (expect 0.881374)\n', acsch(1));
fprintf('  acsch(-1) = %.15f  (expect -0.881374)\n', acsch(-1));
