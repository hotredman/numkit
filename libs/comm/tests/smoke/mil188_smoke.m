clear
import compat.*

fprintf('=== mil188qammod / mil188qamdemod (MIL-STD-188 16-QAM) ===\n');

x = (0:15)';
y = mil188qammod(x, 16);
fprintf('  16-QAM constellation:\n');
for k = 1:16
    fprintf('    y(%2d) = %+.6f%+.6fi\n', k-1, real(y(k)), imag(y(k)));
end

fprintf('\n  Expected (from MATLAB R2025b probe):\n');
fprintf('    y(0)  = +0.866025+0.500000i\n');
fprintf('    y(2)  = +1.000000+0.000000i  (outer ring at 0 deg)\n');
fprintf('    y(3)  = +0.258819+0.258819i  (inner ring at 45 deg)\n');
fprintf('    y(14) = -1.000000+0.000000i  (outer ring at 180 deg)\n');

z = mil188qamdemod(y, 16);
fprintf('\n  round-trip: ');
fprintf('%d ', z);
fprintf(' (expect 0..15)\n');

% Demod with noise
noisy = y + 0.05 * (1 + 1i);
zn = mil188qamdemod(noisy, 16);
fprintf('  noisy demod:  ');
fprintf('%d ', zn);
fprintf(' (expect 0..15)\n');
