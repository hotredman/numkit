clear
import compat.*

% qammod / qamdemod square-QAM constellation order (vs MATLAB R2025b).
% (real/imag are used instead of mat2str — mat2str on complex is a known
% separate gap.)

y = qammod(0:3, 4);
fprintf('q4 real = %s\n', mat2str(real(y)));   % [-1 -1 1 1]
fprintf('q4 imag = %s\n', mat2str(imag(y)));   % [1 -1 1 -1]

y16 = qammod(0:15, 16);
fprintf('q16 imag(1:4) = %s\n', mat2str(imag(y16(1:4))));  % [3 1 -3 -1]

y8 = qammod(0:7, 8);
fprintf('q8 real = %s\n', mat2str(real(y8)));  % [-3 -3 -1 -1 3 3 1 1]

% Unit average power (M=4 → 1/sqrt(2)).
u = qammod(0:3, 4, 'UnitAveragePower', true);
fprintf('uap4 |.| = %.6f\n', abs(u(1)));        % 1.000000

% Round-trip.
z = qamdemod(qammod(0:15, 16), 16);
fprintf('roundtrip16 ok = %d\n', isequal(z(:).', 0:15));   % 1
