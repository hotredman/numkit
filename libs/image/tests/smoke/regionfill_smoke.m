clear
import compat.*

% regionfill — discrete Laplacian inpainting.
% Reference values from MATLAB R2025b (bit-equal via CG tol 1e-12).

I = double(reshape(1:25, 5, 5));

fprintf('=== single-pixel interior mask ===\n');
m1 = false(5, 5); m1(3, 3) = true;
J1 = regionfill(I, m1);
fprintf('J1(3,3) = %g (expect 13 — avg of N+S+W+E = (12+14+8+18)/4)\n', J1(3,3));

fprintf('\n=== 3x3 interior mask (linear field preserved) ===\n');
m2 = false(5,5); m2(2:4, 2:4) = true;
J2 = regionfill(I, m2);
fprintf('J2(2,2) = %g (expect 7)\n',  J2(2,2));
fprintf('J2(3,3) = %g (expect 13)\n', J2(3,3));
fprintf('J2(4,4) = %g (expect 19)\n', J2(4,4));

fprintf('\n=== 4x4 mask on magic(10) (non-linear interior) ===\n');
I3 = double(magic(10));
m3 = false(10,10); m3(4:7, 4:7) = true;
J3 = regionfill(I3, m3);
fprintf('J3(5,5) = %.6f (expect 46.113636)\n', J3(5,5));
fprintf('J3(4,4) = %.6f (expect 29.113636)\n', J3(4,4));

fprintf('\n=== edge-touching mask (3-neighbour stencil) ===\n');
m4 = false(10,10); m4(1:3, 5:7) = true;
J4 = regionfill(I3, m4);
fprintf('J4(1,5) = %.6f (expect 22.450030)\n', J4(1,5));

fprintf('\n=== class preservation ===\n');
Iu = uint8(I);
Ju = regionfill(Iu, m1);
fprintf('class(Ju) = %s  Ju(3,3) = %d (expect uint8 13)\n', class(Ju), Ju(3,3));
