clear

import compat.*

% psf2otf / otf2psf — Point Spread Function ↔ Optical Transfer Function.

fprintf('--- 2-D round-trip ---\n');
psf = fspecial('gaussian', 16, 2.5);
otf = psf2otf(psf);
psf_back = real(otf2psf(otf));
fprintf('size(otf) = %s, size(psf_back) = %s\n', ...
        mat2str(size(otf)), mat2str(size(psf_back)));
fprintf('max|psf - psf_back| = %.4e (expect ~0)\n', ...
        max(abs(psf(:) - psf_back(:))));

fprintf('\n--- 1-D ---\n');
psf1 = [0.1 0.2 0.4 0.2 0.1];
otf1 = psf2otf(psf1);
fprintf('class = %s, size = %s\n', class(otf1), mat2str(size(otf1)));

fprintf('\n--- post-pad to outsize ---\n');
psf2 = [1 2 3];
otf2 = psf2otf(psf2, [1 5]);
fprintf('size = %s (expect [1 5])\n', mat2str(size(otf2)));

fprintf('\n--- centered impulse PSF → constant OTF ---\n');
imp = zeros(8);
% MATLAB convention: psf2otf shifts by -floor(size/2), so an impulse
% at index (floor(N/2)+1, ...) ends up at (1, 1) → fft2 is all 1s.
imp(5, 5) = 1;
otf_imp = psf2otf(imp);
fprintf('all entries equal 1? %d\n', all(abs(otf_imp(:) - 1) < 1e-10));
