clear

% movmean of a COMPLEX array — moving-mean real + imaginary parts separately.
% Fixed 2026-06-05: previously "Not a double array".

m = movmean([1+1i 2+2i 3+3i 4+4i], 2);
fprintf('movmean(.,2) = %g%+gi %g%+gi %g%+gi %g%+gi  (expect 1+1i 1.5+1.5i 2.5+2.5i 3.5+3.5i)\n', ...
        real(m(1)),imag(m(1)), real(m(2)),imag(m(2)), real(m(3)),imag(m(3)), real(m(4)),imag(m(4)));

m3 = movmean([1+1i 5 3-2i 8 2+4i], 3);
fprintf('movmean(.,3) m(3) = %g%+gi  (expect 5.33333-0.66667i)\n', real(m3(3)), imag(m3(3)));

ma = movmean([1+1i 5 3-2i 8], [1 0]);
fprintf('asym [1 0] m(3)   = %g%+gi  (expect 4-1i)\n', real(ma(3)), imag(ma(3)));

M = [1+1i 2; 3 4i; 5+5i 6];
cm = movmean(M, 2);
fprintf('matrix m(2,:)     = %g%+gi, %g%+gi  (expect 2+0.5i, 1+2i)\n', ...
        real(cm(2,1)),imag(cm(2,1)), real(cm(2,2)),imag(cm(2,2)));

r = movmean([1 2 3 4], 2);
fprintf('real movmean(2)   = %g real? %d  (expect 1.5, 1)\n', r(2), isreal(r));
