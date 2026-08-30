clear

% conv of COMPLEX sequences — genuine complex multiply-accumulate (BILINEAR).
% Fixed 2026-06-05: previously "Not a double array".

c = conv([1 1i], [1 1]);
fprintf('conv([1 1i],[1 1])        = %g%+gi %g%+gi %g%+gi  (expect 1, 1+1i, 1i)\n', ...
        real(c(1)),imag(c(1)), real(c(2)),imag(c(2)), real(c(3)),imag(c(3)));

cc = conv([1+1i 2-1i 3i], [2 1i]);
fprintf('cxc c(2),c(3)             = %g%+gi, %g%+gi  (expect 3-1i, 1+8i)\n', ...
        real(cc(2)),imag(cc(2)), real(cc(3)),imag(cc(3)));

cr = conv([1 2 3], [1+1i 1]);
fprintf('complex x real c(2)       = %g%+gi  (expect 3+2i)\n', real(cr(2)), imag(cr(2)));

cs = conv([1+1i 2 3-1i 4], [1 1], 'same');
fprintf('same (n=%d) c(1),c(2)      = %g%+gi, %g%+gi  (expect 3+1i, 5-1i)\n', ...
        numel(cs), real(cs(1)),imag(cs(1)), real(cs(2)),imag(cs(2)));

cv = conv([1+1i 2 3-1i 4], [1 1], 'valid');
fprintf('valid (n=%d) c(1)          = %g%+gi  (expect 3+1i)\n', numel(cv), real(cv(1)), imag(cv(1)));

r = conv([1 2 3], [1 1]);
fprintf('real conv [1 3 5 3] real? %d\n', isreal(r));
