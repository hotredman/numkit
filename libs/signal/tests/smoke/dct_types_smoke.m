clear
import compat.*
% dct / idct Type 1, 3, 4 (orthonormal). Type 2 is the default.
fprintf('dct Type1: '); fprintf('%.6f ', dct([1 2 3 4], 4, 'Type', 1));
fprintf(' (expect 4.927993 -2.140299 0.845510 -0.647395)\n');
fprintf('dct Type3: '); fprintf('%.6f ', dct([1 2 3 4], 4, 'Type', 3));
fprintf(' (expect 4.388955 -3.071930 1.071930 -0.388955)\n');
fprintf('dct Type4: '); fprintf('%.6f ', dct([1 2 3 4], 4, 'Type', 4));
fprintf(' (expect 3.599737 -3.339911 1.771408 -1.658012)\n');

% idct Type 3 equals the forward DCT-II.
fprintf('idct Type3: '); fprintf('%.6f ', idct([1 2 3 4], 4, 'Type', 3));
fprintf(' (expect 5.000000 -2.230442 0.000000 -0.158513)\n');

% Round-trip idct(dct(x,Type t),Type t) == x for every type.
for t = 1:4
    xr = idct(dct([1 2 3 4], 4, 'Type', t), 4, 'Type', t);
    fprintf('Type%d round-trip max|err| = %.2e  (expect ~0)\n', ...
            t, max(abs(xr - [1 2 3 4])));
end
