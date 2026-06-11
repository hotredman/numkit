clear;
import compat.*;

fprintf('--- ycbcr2rgbwide BT.2020 10-bit ---\n');
% YCbCr-encoded outputs from cycle 28 probes; should round-trip back to the
% original RGB inputs:  white [940 940 940], black [64 64 64], mid grey
% [500 500 500], off-grey [800 200 300].
YCC = uint16([940 512 512; 64 512 512; 500 512 512; 364 477 815]);
v = ycbcr2rgbwide(YCC, 10);
for i = 1:size(v,1)
    fprintf('  row %d: R=%4u G=%4u B=%4u\n', i, v(i,1), v(i,2), v(i,3));
end

fprintf('--- ycbcr2rgbwide BT.2020 12-bit ---\n');
YCC12 = uint16([3760 2048 2048; 256 2048 2048; 1602 1721 2324]);
v = ycbcr2rgbwide(YCC12, 12);
for i = 1:size(v,1)
    fprintf('  row %d: R=%4u G=%4u B=%4u\n', i, v(i,1), v(i,2), v(i,3));
end

fprintf('--- H×W×3 image input ---\n');
% These YCbCr values came from cycle 28's probe of (940,64,500) per pixel.
YCCimg = uint16(reshape([320 320 320 610 610 610 942 942 942], 1, 3, 3));
v = ycbcr2rgbwide(YCCimg, 10);
fprintf('  size = [%d %d %d]\n', size(v,1), size(v,2), size(v,3));
for j = 1:size(v,2)
    fprintf('  px %d: R=%4u G=%4u B=%4u\n', j, v(1,j,1), v(1,j,2), v(1,j,3));
end

fprintf('--- Round-trip rgbwide2ycbcr → ycbcr2rgbwide ---\n');
% Expectation: bit-equal recovery of every nominal-range RGB pixel.
RGB0 = double([940 940 940; 64 64 64; 500 500 500; 800 200 300]);
RGB_rt = double(ycbcr2rgbwide(rgbwide2ycbcr(uint16(RGB0), 10), 10));
maxd = max(max(abs(RGB_rt - RGB0)));
fprintf('  10-bit max |delta| over 4 rows: %g (expect 0 or 1 due to rounding)\n', maxd);
