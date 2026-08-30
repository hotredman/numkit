clear

% imgradientxyz / imgradient3 — 3-D directional gradients.
% Reference values from MATLAB R2025b on a 3x4x5 ramp.

V = double(reshape(1:60, 3, 4, 5));

[Gx, Gy, Gz] = imgradientxyz(V);
fprintf('=== imgradientxyz, sobel (default) ===\n');
fprintf('Gx(2,2,2) = %.6g  (expect 132)\n', Gx(2,2,2));
fprintf('Gy(2,2,2) = %.6g  (expect  44)\n', Gy(2,2,2));
fprintf('Gz(2,2,2) = %.6g  (expect 528)\n', Gz(2,2,2));
fprintf('Gx(1,1,1) = %.6g  (expect  66) [replicate boundary]\n', Gx(1,1,1));

[Gxp, Gyp, Gzp] = imgradientxyz(V, 'prewitt');
fprintf('\n=== imgradientxyz, prewitt ===\n');
fprintf('Gxp(2,2,2) = %.6g (expect  54)\n', Gxp(2,2,2));
fprintf('Gyp(2,2,2) = %.6g (expect  18)\n', Gyp(2,2,2));
fprintf('Gzp(2,2,2) = %.6g (expect 216)\n', Gzp(2,2,2));

[Gxc, Gyc, Gzc] = imgradientxyz(V, 'central');
fprintf('\n=== imgradientxyz, central ===\n');
fprintf('Gxc(2,2,2) = %.6g (expect  3)\n', Gxc(2,2,2));
fprintf('Gyc(2,2,2) = %.6g (expect  1)\n', Gyc(2,2,2));
fprintf('Gzc(2,2,2) = %.6g (expect 12)\n', Gzc(2,2,2));

[Gxi, Gyi, Gzi] = imgradientxyz(V, 'intermediate');
fprintf('\n=== imgradientxyz, intermediate (forward diff) ===\n');
fprintf('Gxi(2,2,2) = %.6g (expect  3)\n', Gxi(2,2,2));
fprintf('Gxi(3,4,5) = %.6g (expect  0) [trailing slice zero-padded]\n', Gxi(3,4,5));

[Gmag, Gaz, Gel] = imgradient3(V);
fprintf('\n=== imgradient3, sobel ===\n');
fprintf('Gmag(2,2,2) = %.6g (expect 546.0256)\n',  Gmag(2,2,2));
fprintf('Gaz(2,2,2)  = %.6g (expect -18.4349)\n',  Gaz(2,2,2));
fprintf('Gel(2,2,2)  = %.6g (expect  75.2369)\n',  Gel(2,2,2));

% Single-output form
Gmag1 = imgradient3(V);
fprintf('\n=== imgradient3 single-output ===\n');
fprintf('Gmag1(2,2,2) = %.6g (expect 546.0256)\n', Gmag1(2,2,2));

% From-grads form
[Gx2, Gy2, Gz2] = imgradientxyz(V);
[Gmag2, Gaz2, Gel2] = imgradient3(Gx2, Gy2, Gz2);
fprintf('\n=== imgradient3 from grads ===\n');
fprintf('Gmag2(2,2,2) = %.6g (expect 546.0256)\n', Gmag2(2,2,2));
fprintf('Gaz2(2,2,2)  = %.6g (expect -18.4349)\n', Gaz2(2,2,2));
fprintf('Gel2(2,2,2)  = %.6g (expect  75.2369)\n', Gel2(2,2,2));
