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

fprintf('\n--- M=32 ---\n');
y32 = mil188qammod((0:31)', 32);
fprintf('  32-QAM first 4 entries:\n');
for k = 1:4
    fprintf('    y(%2d) = %+.6f%+.6fi\n', k-1, real(y32(k)), imag(y32(k)));
end
fprintf('  Expected (from MATLAB R2025b probe):\n');
fprintf('    y(0) = +0.866380+0.499386i\n');
fprintf('    y(1) = +0.984849+0.173415i\n');

z32 = mil188qamdemod(y32, 32);
fprintf('\n  round-trip M=32: match = %d (expect 1)\n', isequal((0:31)', z32));

zn32 = mil188qamdemod(y32 + 0.02*(1+1i), 32);
fprintf('  noisy demod M=32: match = %d (expect 1)\n', isequal((0:31)', zn32));

fprintf('\n--- M=64 ---\n');
y64 = mil188qammod((0:63)', 64);
fprintf('  64-QAM spot-check:\n');
fprintf('    y(0)  = %+.6f%+.6fi  (expect +1.0+0i)\n', real(y64(1)), imag(y64(1)));
fprintf('    y(10) = %+.6f%+.6fi  (expect +0.588429+0.117686i)\n', real(y64(11)), imag(y64(11)));
fprintf('    y(63) = %+.6f%+.6fi  (expect -0.353057-0.353057i)\n', real(y64(64)), imag(y64(64)));

z64 = mil188qamdemod(y64, 64);
fprintf('\n  round-trip M=64: match = %d (expect 1)\n', isequal((0:63)', z64));

zn64 = mil188qamdemod(y64 + 0.01*(1+1i), 64);
fprintf('  noisy demod M=64: match = %d (expect 1)\n', isequal((0:63)', zn64));

fprintf('\n--- M=256 (largest, closes cluster) ---\n');
y256 = mil188qammod((0:255)', 256);
fprintf('  256-QAM spot-check:\n');
fprintf('    y(0)   = %+.6f%+.6fi  (expect +0.959366+0.056433i)\n', real(y256(1)), imag(y256(1)));
fprintf('    y(127) = %+.6f%+.6fi\n', real(y256(128)), imag(y256(128)));
fprintf('    y(255) = %+.6f%+.6fi  (expect -0.282166-0.282166i)\n', real(y256(256)), imag(y256(256)));

z256 = mil188qamdemod(y256, 256);
fprintf('\n  round-trip M=256: match = %d (expect 1)\n', isequal((0:255)', z256));

zn256 = mil188qamdemod(y256 + 0.005*(1+1i), 256);
fprintf('  noisy demod M=256: match = %d (expect 1)\n', isequal((0:255)', zn256));
