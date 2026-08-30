clear

% imresize bilinear/bicubic interpolation -- bugs/image/imresize-interp.
% MATLAB convention: pixel-centre coordinate map + mirror boundary + antialiasing
% on shrink. Default method = bicubic.

rb = imresize([1 2; 3 4], 2, 'bilinear');
fprintf('bilinear x2: (1,1)=%.4f (1,2)=%.4f (4,4)=%.4f   (expect 1.0000 1.2500 4.0000)\n', ...
        rb(1,1), rb(1,2), rb(4,4));

rc = imresize([1 2; 3 4], 2, 'bicubic');
fprintf('bicubic  x2: (1,1)=%.5f   (expect 0.71875)\n', rc(1,1));

d = imresize([1 2 3 4 5 6], [1 3]);     % default bicubic + antialiasing (downscale)
fprintf('downscale [1 3]: %s   (expect 1.44922 3.50000 5.55078)\n', num2str(d, '%.5f '));

e = imresize([1 2; 3 4], 2, 'nearest');
fprintf('nearest  x2: (1,1)=%g (4,4)=%g   (expect 1 4)\n', e(1,1), e(4,4));
