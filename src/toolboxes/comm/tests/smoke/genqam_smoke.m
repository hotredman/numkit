clear

fprintf('=== genqammod / genqamdemod (generic QAM constellation) ===\n');

% 8-PSK constellation
M = 8;
const = exp(1i * 2*pi*(0:M-1)/M);
x = (0:7)';
y = genqammod(x, const);
fprintf('  8-PSK genqammod, indices [0..7]:\n');
for k = 1:length(y)
    fprintf('    y(%d) = %.6f + %.6fi\n', k, real(y(k)), imag(y(k)));
end

% Round-trip
z = genqamdemod(y, const);
fprintf('\n  genqamdemod round-trip: ');
fprintf('%d ', z);
fprintf(' (expect 0..7)\n');

% Custom 4-point: [1, i, -1, -i]
const2 = [1+0i, 0+1i, -1+0i, 0-1i];
x2 = [0 1 2 3 0 1]';
y2 = genqammod(x2, const2);
fprintf('\n  custom [1, i, -1, -i] genqammod:\n');
for k = 1:length(y2)
    fprintf('    y(%d) = %.4f + %.4fi\n', k, real(y2(k)), imag(y2(k)));
end
z2 = genqamdemod(y2, const2);
fprintf('  round-trip: ');
fprintf('%d ', z2);
fprintf(' (expect 0 1 2 3 0 1)\n');

% Real PAM constellation
const3 = [-3 -1 1 3];
y3 = genqammod([0 1 2 3]', const3);
fprintf('\n  PAM [-3 -1 1 3] genqammod: ');
fprintf('%g ', y3);
fprintf('\n');
z3 = genqamdemod(y3, const3);
fprintf('  round-trip: ');
fprintf('%d ', z3);
fprintf(' (expect 0 1 2 3)\n');

% Demod with noise — nearest neighbor
noisy = y + 0.05 * (1 + 1i);
z4 = genqamdemod(noisy, const);
fprintf('\n  demod with small noise: ');
fprintf('%d ', z4);
fprintf(' (expect 0..7)\n');
