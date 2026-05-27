clear
import compat.*

% deconvreg — Tikhonov-regularized FFT-based deconvolution.
% Reference values from MATLAB R2025b on the no-noise checkerboard.

I = checkerboard(8);
PSF = fspecial('gaussian', 7, 10);
B = imfilter(I, PSF, 'circular');

fprintf('=== 2-arg default (NP=0) ===\n');
J1 = deconvreg(B, PSF);
fprintf('J1(1,1)   = %.6g (expect 0.2370)\n', J1(1,1));
fprintf('J1(32,32) = %.6g (expect 0.2370)\n', J1(32,32));

fprintf('\n=== 3-arg (NP=0.01, default LRANGE search) ===\n');
[J4, L4] = deconvreg(B, PSF, 0.01);
fprintf('J4(1,1)   = %.6g (expect 0.2370)\n', J4(1,1));
fprintf('LAGRA4    = %.6g (expect 3.487e-05)\n', L4);

fprintf('\n=== 4-arg scalar LRANGE (fixed lambda=0.5) ===\n');
[J9, L9] = deconvreg(B, PSF, 0.01, 0.5);
fprintf('J9(1,1)   = %.6g (expect 0.3974)\n', J9(1,1));
fprintf('LAGRA9    = %.6g (expect 0.5)\n', L9);

fprintf('\n=== 4-arg [lo hi] LRANGE (Brent fminbnd search) ===\n');
[Jb, Lb] = deconvreg(B, PSF, 0.01, [1e-9, 1e9]);
fprintf('Jb(1,1)   = %.6g (expect 0.2370)\n', Jb(1,1));
fprintf('LAGRA_b   = %.6g (expect 3.487e-05)\n', Lb);

fprintf('\n=== 5-arg custom 2-D REGOP ===\n');
regop2 = [1 -1 0; -1 0 1; 0 1 -1];
[Jr, Lr] = deconvreg(B, PSF, 0.01, [1e-9, 1e9], regop2);
fprintf('Jr(1,1)   = %.6g (expect 0.1892)\n', Jr(1,1));
fprintf('LAGRA_r   = %.6g (expect 0.000289)\n', Lr);
