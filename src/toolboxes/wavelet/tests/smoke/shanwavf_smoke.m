clear

fprintf('=== shanwavf ===\n');
[psi, x] = shanwavf(-5, 5, 8, 1, 1);
fprintf('  shanwavf(-5,5,8,1,1) real psi: '); fprintf('%.4f ', real(psi)); fprintf('\n');

[psi, x] = shanwavf(-4, 4, 33, 1, 1);
fprintf('  N=33: psi(17) at t=0 = %.4f + %gi (real, peak)\n', real(psi(17)), imag(psi(17)));

[psi, x] = shanwavf(-5, 5, 16, 0.5, 2);
fprintf('  fb=0.5 fc=2: real psi(8) = %.4f\n', real(psi(8)));
