clear

% imbinarize now accepts the method-string forms imbinarize(I,'global') and
% imbinarize(I,'adaptive', ...). 'global' is the default Otsu threshold;
% 'adaptive' rewires to adaptthresh(I, Sensitivity) and then the per-pixel
% binarize comparison (BW = I > class-range-scaled T). numkit previously
% misread 'adaptive' as a per-pixel threshold matrix and threw a shape error.
% Optional NV: 'Sensitivity' (default 0.5), 'ForegroundPolarity' ('bright').

J = uint8(mod((1:16)' * (1:16), 256));   % deterministic 16x16 image

fprintf('--- ''global'' == default Otsu ---\n');
bg = imbinarize(J, 'global');
bd = imbinarize(J);
fprintf('global sum=%d   global==default? %d   (expect 78, 1)\n', ...
        sum(bg(:)), isequal(bg, bd));

fprintf('--- ''adaptive'' (default Sensitivity 0.5) ---\n');
ba = imbinarize(J, 'adaptive');
fprintf('adaptive sum=%d   ba(8,8)=%d   (expect 3, 0)\n', sum(ba(:)), double(ba(8,8)));

fprintf('--- ''adaptive'' with Sensitivity 0.7 ---\n');
bs = imbinarize(J, 'adaptive', 'Sensitivity', 0.7);
fprintf('sum=%d   (expect 224 - more foreground at higher sensitivity)\n', sum(bs(:)));

fprintf('--- scalar / per-pixel threshold forms still work ---\n');
bt = imbinarize(J, 0.5);
fprintf('scalar-T sum=%d\n', sum(bt(:)));
