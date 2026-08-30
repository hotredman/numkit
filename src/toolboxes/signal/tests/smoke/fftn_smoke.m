clear

fprintf('=== signal/fftn / ifftn — N-D Fast Fourier Transform ===\n');

fprintf('\n[2-D matrix — matches fft2]\n');
X = [1 2 3; 4 5 6; 7 8 9];
Y = fftn(X);
fprintf('  Y(1,1) = %.4f + %.4fi   (DC bin = sum(X) = 45)\n', ...
    real(Y(1,1)), imag(Y(1,1)));

fprintf('\n[3-D array  reshape(1:24, 2, 3, 4)]\n');
X = reshape(1:24, 2, 3, 4);
Y = fftn(X);
fprintf('  size(Y) = [%d %d %d]\n', size(Y, 1), size(Y, 2), size(Y, 3));
fprintf('  Y(1,1,1) = %.4f + %.4fi  (expect 300 + 0i)\n', ...
    real(Y(1,1,1)), imag(Y(1,1,1)));
fprintf('  Y(1,1,2) = %.4f + %.4fi  (expect -72 + 72i)\n', ...
    real(Y(1,1,2)), imag(Y(1,1,2)));
fprintf('  Y(1,1,4) = %.4f + %.4fi  (expect -72 - 72i)\n', ...
    real(Y(1,1,4)), imag(Y(1,1,4)));

fprintf('\n[ifftn round-trip identity]\n');
Xr = ifftn(fftn(X));
fprintf('  max |X - ifftn(fftn(X))| = %.2e  (expect ~ulp)\n', ...
    max(abs(X(:) - Xr(:))));

fprintf('\n[size override — zero-pad to [4 4]]\n');
Y = fftn([1 2; 3 4], [4 4]);
fprintf('  size(Y) = [%d %d]   Y(1,1) = %.4f  (expect 10 = 1+2+3+4)\n', ...
    size(Y, 1), size(Y, 2), real(Y(1,1)));

fprintf('\nBit-equal (~ulp) with MATLAB R2025b. Octave 11.1.0 ships in core.\n');
