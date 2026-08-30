clear

% rgb2ntsc / ntsc2rgb — RGB ↔ YIQ (NTSC) linear color space.

fprintf('--- rgb2ntsc primaries ---\n');
disp(rgb2ntsc([1 0 0]));   % expect [.299  .596  .211]
disp(rgb2ntsc([0 1 0]));   % expect [.587 -.274 -.523]
disp(rgb2ntsc([0 0 1]));   % expect [.114 -.322  .312]

fprintf('--- round-trip on random colormap ---\n');
rgb_map = reshape((0:11)/11, [4 3]);
yiq = rgb2ntsc(rgb_map);
back = ntsc2rgb(yiq);
fprintf('max|round-trip error| = %.4e (expect <1e-3)\n', ...
        max(abs(rgb_map(:) - back(:))));

fprintf('\n--- on H×W×3 image ---\n');
RGB = cat(3, ones(2,2), 0.5*ones(2,2), zeros(2,2));   % yellow-ish
YIQ = rgb2ntsc(RGB);
fprintf('size: %s\n', mat2str(size(YIQ)));
fprintf('Y(1,1)=%.4f I(1,1)=%.4f Q(1,1)=%.4f\n', ...
        YIQ(1,1,1), YIQ(1,1,2), YIQ(1,1,3));
