clear

import compat.*

fprintf('=== cmorwavf ===\n');
% Bug fix 2026-05-08: 3-arg form was throwing
[psi, x] = cmorwavf(-5, 5, 8);
fprintf('  cmorwavf(-5,5,8) defaults fb=fc=1:\n');
fprintf('    real psi(4)=%.4f imag=%.4f\n', real(psi(4)), imag(psi(4)));

[psi, x] = cmorwavf(-5, 5, 8, 1.5, 1);
fprintf('  cmorwavf(-5,5,8,1.5,1):\n');
fprintf('    real psi(4)=%.4f imag=%.4f\n', real(psi(4)), imag(psi(4)));

[psi, x] = cmorwavf(-4, 4, 33);
fprintf('  N=33 |psi(17)| (peak) = %.4f (= 1/√π ≈ 0.5642)\n', abs(psi(17)));
