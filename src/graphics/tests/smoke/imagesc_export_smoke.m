clear;

% imagesc / imshow export (portion 35) — engine-side PNG/SVG rendering
% of image datasets with the MATLAB colormap mapping.
% NOTE: file-existence probes go through an ASSIGNMENT (n = numel(dir(
% ...))); a call in bare-argument position hits the nargout bug
% bugs/opened/runtime/dir-nargout-in-argument.md and would print 0.

C = [1 2 3 4; 5 6 7 8; 9 10 NaN 12];
figure;
imagesc(C);
saveas(gcf, [tempdir 'smoke_imagesc.png']);
n = numel(dir([tempdir 'smoke_imagesc.png']));
fprintf('imagesc png exists = %d (expect 1)\n', n);

saveas(gcf, [tempdir 'smoke_imagesc.svg'], 'svg');
svg = fileread([tempdir 'smoke_imagesc.svg']);
fprintf('svg embeds image = %d (expect 1)\n', contains(svg, 'data:image/png;base64,'));

% Axes state MATLAB semantics (probe-verified R2025b).
fprintf('YDir = %s (expect reverse)\n', get(gca, 'YDir'));
cl = get(gca, 'CLim');
fprintf('CLim = [%g %g] (expect [1 12], NaN excluded)\n', cl(1), cl(2));

% clim window + custom palette endpoints.
clim([2 11]);
colormap(gray(4));
fprintf('CLim override = [%g %g] (expect [2 11])\n', get(gca, 'CLim'));

% imshow grayscale + truecolor export.
close all;
figure;
imshow(uint8([0 64 128; 192 255 32]));
saveas(gcf, [tempdir 'smoke_imshow.png']);
n = numel(dir([tempdir 'smoke_imshow.png']));
fprintf('imshow png exists = %d (expect 1)\n', n);

close all;
figure;
I = zeros(2, 3, 3, 'uint8');
I(1,1,:) = [255 0 0];
I(1,2,:) = [0 255 0];
I(1,3,:) = [0 0 255];
I(2,1,:) = [255 255 0];
I(2,2,:) = [0 255 255];
I(2,3,:) = [255 0 255];
imshow(I);
saveas(gcf, [tempdir 'smoke_rgb.png']);
n = numel(dir([tempdir 'smoke_rgb.png']));
fprintf('rgb imshow png exists = %d (expect 1)\n', n);
