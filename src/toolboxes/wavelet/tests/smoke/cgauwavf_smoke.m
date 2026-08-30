clear

% cgauwavf — complex Gaussian wavelet (trapezoidal L^2 normalisation).

[psi, x] = cgauwavf(-5, 5, 11);
fprintf('p=1 (default), at t=-1:\n');
fprintf('  real = %.4f (expect 0.4598)\n', real(psi(5)));
fprintf('  imag = %.4f (expect 0.2734)\n', imag(psi(5)));

[psi, x] = cgauwavf(-5, 5, 11, 2);
fprintf('p=2, at t=0:\n');
fprintf('  real = %.4f (expect -0.8088)\n', real(psi(6)));
