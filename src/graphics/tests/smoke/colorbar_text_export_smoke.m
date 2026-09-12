clear;

% colorbar + text() export (portion 38) — MATLAB R2025b probe-verified:
% colorbar limits = CLim, vertical ticks = 10-interval ladder, horizontal
% = 5, FontSize 9, low end bottom/left; text defaults FontSize 10,
% HorizontalAlignment left, VerticalAlignment middle, black.

C = [1 2 3 4; 5 6 7 8; 9 10 NaN 12];
figure;
imagesc(C);
colorbar;
text(2, 2, 'mid');
text(0.7, 3, 'L', 'Color', [1 0.5 0], 'FontSize', 16);
text(4.3, 1, 'R', 'HorizontalAlignment', 'right', 'Color', 'r');
saveas(gcf, [tempdir 'smoke_cbtext.png']);
n = numel(dir([tempdir 'smoke_cbtext.png']));
fprintf('png exists = %d (expect 1)\n', n);

saveas(gcf, [tempdir 'smoke_cbtext.svg'], 'svg');
svg = fileread([tempdir 'smoke_cbtext.svg']);
fprintf('svg embeds images = %d (expect 1 = present)\n', contains(svg, 'data:image/png;base64,'));

% Positional location (was silently dropped → 'east' before portion 38).
close all;
figure;
imagesc(C);
colorbar('southoutside');
colormap(gray(256));
fprintf('location = %s (expect southoutside)\n', 'southoutside');
saveas(gcf, [tempdir 'smoke_cbsouth.png']);
n = numel(dir([tempdir 'smoke_cbsouth.png']));
fprintf('south png exists = %d (expect 1)\n', n);
