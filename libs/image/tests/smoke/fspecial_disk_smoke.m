clear
import compat.*

% fspecial('disk',r) builds the circular averaging filter. MATLAB R2025b
% uses the EXACT sub-pixel area-coverage integral: each cell's weight is
% the fraction of that unit cell lying inside the disk of radius r. numkit
% previously used a crude linear-taper approximation, so the coefficients
% (and the kernel size for non-integer r) were off.

h2 = fspecial('disk', 2);
fprintf('--- fspecial(''disk'', 2) ---\n');
fprintf('size=%dx%d sum=%.6f\n', size(h2,1), size(h2,2), sum(h2(:)));
fprintf('centre h(3,3)=%.10f   (expect 0.0795774715)\n', h2(3,3));
fprintf('corner h(1,1)=%.10f   (expect 0)\n', h2(1,1));
fprintf('mid-edge h(3,1)=%.10f (expect 0.0381149714)\n', h2(3,1));
fprintf('h(2,2)=%.10f          (expect 0.0783813542)\n', h2(2,2));

h3 = fspecial('disk', 3);
fprintf('--- fspecial(''disk'', 3) ---\n');
fprintf('size=%dx%d centre h(4,4)=%.10f (expect 7x7, 0.0353677651)\n', ...
        size(h3,1), size(h3,2), h3(4,4));

h5 = fspecial('disk', 5);
fprintf('--- fspecial(''disk'', 5) ---\n');
fprintf('size=%dx%d centre h(6,6)=%.10f (expect 11x11, 0.0127323954)\n', ...
        size(h5,1), size(h5,2), h5(6,6));

% Non-integer radius: size = 2*ceil(r-0.5)+1 (MATLAB), so r=2.4 -> 5x5.
h24 = fspecial('disk', 2.4);
fprintf('--- fspecial(''disk'', 2.4) ---\n');
fprintf('size=%dx%d sum=%.6f (expect 5x5, 1)\n', size(h24,1), size(h24,2), sum(h24(:)));
