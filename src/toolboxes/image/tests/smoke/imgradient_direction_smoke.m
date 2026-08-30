clear

% imgradientxy / imgradient now use MATLAB's sobel/prewitt sign convention:
%   Gx > 0 where intensity increases left->right
%   Gy > 0 where intensity increases top->bottom
%   imgradient Gdir = atan2(-Gy, Gx) in degrees [-180,180]
% Previously the 2-D sobel/prewitt kernels were sign-flipped, so Gx/Gy were
% negated and Gdir came out 180 deg wrong (168.69 instead of -11.31).

A = reshape(1:25,5,5);   % rises left->right (by 5) and top->bottom (by 1)

[gx, gy] = imgradientxy(A);
fprintf('imgradientxy sobel  Gx(3,3)=%.4f Gy(3,3)=%.4f  (expect +40, +8)\n', gx(3,3), gy(3,3));

[gm, gd] = imgradient(A);
fprintf('imgradient sobel    Gmag(3,3)=%.6f Gdir(3,3)=%.6f  (expect 40.792156, -11.309932)\n', gm(3,3), gd(3,3));

[gxp, gyp] = imgradientxy(A, 'prewitt');
fprintf('imgradientxy prewitt Gx(3,3)=%.4f Gy(3,3)=%.4f  (expect +30, +6)\n', gxp(3,3), gyp(3,3));

[gmp, gdp] = imgradient(A, 'prewitt');
fprintf('imgradient prewitt  Gdir(3,3)=%.6f  (expect -11.309932)\n', gdp(3,3));

[gmc, gdc] = imgradient(A, 'central');
fprintf('imgradient central  Gdir(3,3)=%.6f  (expect -11.309932, was already correct)\n', gdc(3,3));
