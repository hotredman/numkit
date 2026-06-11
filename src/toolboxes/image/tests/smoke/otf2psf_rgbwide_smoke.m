clear;
import compat.*;

% ─── otf2psf round-trip + outsize-crop verification ────────────────

fprintf('--- otf2psf 3x3 round-trip ---\n');
psf = [1 2 3; 4 5 6; 7 8 9];
disp(real(otf2psf(psf2otf(psf))));

fprintf('\n--- otf2psf 4x4 round-trip (even) ---\n');
disp(real(otf2psf(psf2otf(reshape(1:16, 4, 4)))));

fprintf('\n--- otf2psf with outsize crop (PSF padded 2x2->5x5, crop back to [2 2]) ---\n');
otf = psf2otf([1 2; 3 4], [5 5]);
disp(real(otf2psf(otf, [2 2])));

fprintf('\n--- otf2psf 1-D ---\n');
disp(real(otf2psf(psf2otf([1 2 3], [1 7]), [1 3])));

fprintf('\n--- otf2psf outsize > size(otf) throws ---\n');
try
    otf2psf(otf, [6 6]);
    fprintf('  NO ERROR (unexpected)\n');
catch e
    fprintf('  ERROR (expected): %s\n', e.message);
end

% ─── rgbwide2ycbcr ──────────────────────────────────────────────────

fprintf('\n--- rgbwide2ycbcr BT.2020 10-bit ---\n');
RGB10 = uint16([940 940 940; 64 64 64; 500 500 500; 800 200 300]);
v = rgbwide2ycbcr(RGB10, 10);
for i = 1:size(v,1)
    fprintf('  row %d: Y=%4u Cb=%4u Cr=%4u\n', i, v(i,1), v(i,2), v(i,3));
end

fprintf('--- rgbwide2ycbcr BT.2020 12-bit ---\n');
RGB12 = uint16([3760 3760 3760; 256 256 256; 2000 1500 1000]);
v = rgbwide2ycbcr(RGB12, 12);
for i = 1:size(v,1)
    fprintf('  row %d: Y=%4u Cb=%4u Cr=%4u\n', i, v(i,1), v(i,2), v(i,3));
end

fprintf('--- rgbwide2ycbcr H×W×3 image input ---\n');
RGBimg = uint16(reshape([940 940 940 64 64 64 500 500 500], 1, 3, 3));
vimg = rgbwide2ycbcr(RGBimg, 10);
fprintf('  size = [%d %d %d]\n', size(vimg,1), size(vimg,2), size(vimg,3));
for j = 1:size(vimg,2)
    fprintf('  px %d: Y=%4u Cb=%4u Cr=%4u\n', j, vimg(1,j,1), vimg(1,j,2), vimg(1,j,3));
end
