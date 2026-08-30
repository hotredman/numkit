clear

fprintf('=== fbspwavf ===\n');
[psi, x] = fbspwavf(-5, 5, 8, 2, 1, 1);
fprintf('  m=2 fb=1 fc=1, N=8 real psi: '); fprintf('%.4f ', real(psi)); fprintf('\n');

[psi, x] = fbspwavf(-5, 5, 16, 3, 1, 1);
fprintf('  m=3 N=16 real psi(8) = %.4f\n', real(psi(8)));

[psi, x] = fbspwavf(-4, 4, 33, 2, 1, 1);
fprintf('  N=33: psi(17) at t=0 = %.4f + %gi (real, peak)\n', real(psi(17)), imag(psi(17)));
