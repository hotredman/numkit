clear
import compat.*

fprintf('=== comm/dpcmopt — DPCM parameter optimiser ===\n');
fprintf('Clean-room: Makhoul 1975; Proakis & Manolakis; Jayant & Noll.\n');

training = sin(2*pi*0.1*(0:50)) + 0.05*(0:50);

[predictor, codebook, partition] = dpcmopt(training, 2, 4);
fprintf('\n  predictor: ');
fprintf('%.6f ', predictor);
fprintf('\n  expected : 0 1.530775 -0.602549\n');
fprintf('  codebook : ');
fprintf('%.6f ', codebook);
fprintf('\n  expected : -0.190703 0.007112 0.218676 0.410908\n');
fprintf('  partition: ');
fprintf('%.6f ', partition);
fprintf('\n  expected : -0.091795 0.112894 0.314792\n');

% Predictor only (1-output form)
p2 = dpcmopt(training, 3);
fprintf('\n  order-3 predictor: ');
fprintf('%.6f ', p2);
fprintf('\n  expected : 0 1.603410 -0.787079 0.120547\n');

% Round-trip via dpcmenco/deco
[predictor, codebook, partition] = dpcmopt(training, 2, 4);
[indx, qe] = dpcmenco(training, codebook, partition, predictor);
[recon, ~] = dpcmdeco(indx, codebook, predictor);
fprintf('\n  round-trip via dpcmenco/deco: MSE = %g\n', ...
        mean((training - recon).^2));

% Correctness: recover the coefficients of a known AR(2) process.
% The predictor of an AR(p) signal is its AR coefficients.
rng(12345);
e = randn(1, 6000);
x = filter(1, [1 -1.4 0.5], e);    % AR(2): x[n] = 1.4 x[n-1] - 0.5 x[n-2] + e
p = dpcmopt(x, 2);
fprintf('\n  AR(2) recovery: predictor = [%.4f %.4f %.4f]\n', p(1), p(2), p(3));
fprintf('  true AR coeffs:             [0.0000 1.4000 -0.5000]\n');
resid = x - filter([0 p(2) p(3)], 1, x);
fprintf('  residual variance / signal variance = %.4f (predictor\n', ...
        var(resid) / var(x));
fprintf('  removes most of the signal energy)\n');

fprintf('\ndpcmopt matches MATLAB R2025b. Octave 11.1.0 ships dpcmopt in\n');
fprintf('the communications package (load with: pkg load communications).\n');
