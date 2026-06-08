clear;
import compat.*;

I = double([0.1 0.2 0.3; 0.4 0.5 0.6; 0.7 0.8 0.9]);
PSF = ones(3) / 9;

fprintf('--- (1) NSR = 0 (ideal inverse) ---\n');
disp(deconvwnr(I, PSF, 0));

fprintf('--- (2) NSR = 0.01 (regularised) ---\n');
disp(deconvwnr(I, PSF, 0.01));

fprintf('--- (3) NSR = 0.1 (strongly regularised) ---\n');
disp(deconvwnr(I, PSF, 0.1));

fprintf('--- (4) (NCORR=0.01, ICORR=1) ≡ NSR=0.01 ---\n');
disp(deconvwnr(I, PSF, 0.01, 1));

fprintf('--- (5) uint8 input — class-preserving ---\n');
Iu = uint8([10 20 30; 40 50 60; 70 80 90]);
Ju = deconvwnr(Iu, PSF, 0.01);
fprintf('  class(Ju) = %s\n', class(Ju));
for r = 1:3
    fprintf('  '); for c = 1:3; fprintf('%4u', Ju(r,c)); end; fprintf('\n');
end

fprintf('--- (6) realistic blur + deconv (PSF smaller than I) ---\n');
PSF3 = fspecial('gaussian', 7, 1.5);
Iorig = double(reshape(1:100, 10, 10));
H = psf2otf(PSF3, [10 10]);
Iblur = real(ifft2(fft2(Iorig) .* H));
J = deconvwnr(Iblur, PSF3, 0);
fprintf('  max |J − Iorig| (NSR=0) = %.6e (expect ~1e-12)\n', ...
        max(max(abs(J - Iorig))));
