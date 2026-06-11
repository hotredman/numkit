clear;
import compat.*;

I = double(reshape(1:64, 8, 8));
PSF = fspecial('gaussian', 3, 1);

fprintf('--- (1) 8x8 image, 3x3 Gaussian PSF ---\n');
J = edgetaper(I, PSF);
fprintf('  J(1,1) = %.6f  (expect ~20.733; tapered to image mean 32.5)\n', J(1,1));
fprintf('  J(4,4) = %.6f  (expect 28.000; centre preserved exactly)\n', J(4,4));
fprintf('  J(8,8) = %.6f  (expect ~44.267)\n', J(8,8));

fprintf('\n--- (2) uint8 input — class-preserving ---\n');
Iu = uint8(I);
Ju = edgetaper(Iu, PSF);
fprintf('  class(Ju) = %s\n', class(Ju));
fprintf('  Ju(1,1)=%u Ju(4,4)=%u Ju(8,8)=%u (expect 21, 28, 44)\n', ...
        Ju(1,1), Ju(4,4), Ju(8,8));

fprintf('\n--- (3) constant image: J ≡ I  (alpha-symmetric edge case) ---\n');
I2 = ones(8, 8) * 0.5;
J = edgetaper(I2, PSF);
fprintf('  range = [%.6e, %.6e]  (expect [0.5, 0.5])\n', ...
        min(J(:)), max(J(:)));

fprintf('\n--- (4) PSF too large → error ---\n');
try
    edgetaper(zeros(4, 4), ones(3, 3));
    fprintf('  NO ERROR (unexpected)\n');
catch e
    fprintf('  ERROR (expected): %s\n', e.message);
end
