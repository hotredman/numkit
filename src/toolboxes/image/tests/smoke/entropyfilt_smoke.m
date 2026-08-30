clear

% entropyfilt — local Shannon entropy filter.

fprintf('--- uniform (all entropy 0) ---\n');
disp(entropyfilt(uint8(ones(5, 5))));

fprintf('\n--- magic(5) with 3x3 domain (Octave reference) ---\n');
% Expected from Octave-source (3x3 ones domain):
%   border: -(2*log2(2/9) + log2(1/9))/3 ≈ 1.5301
%   corner: -(4*log2(4/9) + 4*log2(2/9) + log2(1/9))/9 ≈ 1.6577
%   inner: log2(9) ≈ 3.1699
M = uint8([17 24 1 8 15; 23 5 7 14 16; 4 6 13 20 22; 10 12 19 21 3; 11 18 25 2 9]);
E = entropyfilt(M, ones(3, 3));
fprintf('inner (2,2) = %.6f (expect ≈ 3.1699 = log2(9))\n', E(2, 2));
fprintf('border (1,2) = %.6f (expect ≈ 1.5301)\n', E(1, 2));
fprintf('corner (1,1) = %.6f (expect ≈ 1.6577)\n', E(1, 1));

fprintf('\n--- default 9x9 domain on a 5x5 image ---\n');
E9 = entropyfilt(M);
fprintf('size = %s, range = [%.4f, %.4f]\n', mat2str(size(E9)), ...
        min(E9(:)), max(E9(:)));
