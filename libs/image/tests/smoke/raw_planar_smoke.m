clear
import compat.*

% raw2planar / planar2raw — Bayer CFA deinterleave smoke.
% Reference: MATLAB R2025b Image Processing Toolbox.

I = uint8(reshape(1:64, 8, 8));
P = raw2planar(I);
fprintf('== uint8 raw2planar ==\n');
fprintf('  size = [%d %d %d]  (expect 4 4 4)\n', size(P,1), size(P,2), size(P,3));
fprintf('  P(1,1,:) = [%d %d %d %d]  (expect 1 9 2 10)\n', P(1,1,1), P(1,1,2), P(1,1,3), P(1,1,4));
fprintf('  P(2,2,:) = [%d %d %d %d]  (expect 19 27 20 28)\n', P(2,2,1), P(2,2,2), P(2,2,3), P(2,2,4));

fprintf('\n== distinguishable mosaic ==\n');
I = uint8(zeros(8,8));
I(1:2:end,1:2:end) = 100;
I(1:2:end,2:2:end) = 50;
I(2:2:end,1:2:end) = 60;
I(2:2:end,2:2:end) = 200;
P = raw2planar(I);
fprintf('  P(1,1,:) = [%d %d %d %d]  (expect 100 50 60 200)\n', P(1,1,1), P(1,1,2), P(1,1,3), P(1,1,4));

fprintf('\n== round-trip ==\n');
back = planar2raw(P);
fprintf('  isequal(I, back) = %d\n', isequal(I, back));
fprintf('  class(back) = %s (expect uint8)\n', class(back));

fprintf('\n== uint16 ==\n');
P16 = raw2planar(uint16(reshape(1:64, 8, 8)));
fprintf('  class(P16) = %s\n', class(P16));

fprintf('\n== double ==\n');
Pd = raw2planar(double(reshape(1:36, 6, 6)));
fprintf('  class(Pd) = %s  size = [%d %d %d]\n', class(Pd), size(Pd,1), size(Pd,2), size(Pd,3));

fprintf('\n== planar2raw direct ==\n');
P = uint8(zeros(3,3,4));
P(:,:,1) = 100;  P(:,:,2) = 50;  P(:,:,3) = 60;  P(:,:,4) = 200;
cfa = planar2raw(P);
fprintf('  size = [%d %d]  (expect 6 6)\n', size(cfa,1), size(cfa,2));
fprintf('  cfa(1:2,1:2) = [%d %d; %d %d]  (expect 100 50; 60 200)\n', ...
    cfa(1,1), cfa(1,2), cfa(2,1), cfa(2,2));
