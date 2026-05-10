clear
import compat.*

fprintf('=== dpcmopt (DPCM parameter optimiser) ===\n');

training = sin(2*pi*0.1*(0:50)) + 0.05*(0:50);

[predictor, codebook, partition] = dpcmopt(training, 2, 4);
fprintf('  predictor: ');
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
