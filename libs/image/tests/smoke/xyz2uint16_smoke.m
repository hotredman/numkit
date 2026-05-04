clear

import compat.*

% xyz2uint16 — XYZ double → uint16 (ICC encoding).

fprintf('--- double XYZ ---\n');
disp(double(xyz2uint16([0 0.5 1.0; 0.25 0.75 1.99997; 1.5 0 -0.1])));
fprintf('  expect [0 16384 32768; 8192 24576 65535; 49152 0 0]\n\n');

fprintf('--- uint16 passthrough ---\n');
disp(double(xyz2uint16(uint16([0 32768 65535]))));
fprintf('  expect [0 32768 65535]\n\n');

fprintf('--- round-trip xyz2double/xyz2uint16 ---\n');
A = [0.1 0.5 1.5];
B = xyz2double(xyz2uint16(A));
fprintf('A: %s\n', mat2str(A, 6));
fprintf('B: %s\n', mat2str(B, 6));
fprintf('max|diff| = %.2e (expect <= 1/32768/2 ≈ 1.5e-5)\n', max(abs(A-B)));
