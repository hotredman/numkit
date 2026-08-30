clear

fprintf('=== morlet ===\n');
[psi, x] = morlet(-5, 5, 8);
fprintf('  morlet(-5, 5, 8):\n');
fprintf('    psi: '); fprintf('%.4g ', psi); fprintf('\n');
fprintf('    x  : '); fprintf('%g ', x); fprintf('\n');

[psi16, x16] = morlet(-5, 5, 16);
fprintf('  morlet(-5, 5, 16) center: %.4f\n', psi16(8));

[psi_a, x_a] = morlet(0, 5, 16);
fprintf('  morlet(0, 5, 16) at t=0: %g (= 1, peak amplitude)\n', psi_a(1));
