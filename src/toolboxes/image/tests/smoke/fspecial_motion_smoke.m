clear

% fspecial('motion',len,theta) builds an anti-aliased motion-blur PSF: a
% line of `len` pixels at `theta` degrees (counter-clockwise from
% horizontal), normalised to sum 1. MATLAB R2025b. numkit previously threw
% 'unknown filter type 'motion''. (Fixing it also exposed a fspecial_reg
% bug: a scalar 2nd arg was size-doubled for EVERY filter, so motion's
% theta silently received the len value.)

m0 = fspecial('motion', 9, 0);            % horizontal blur, default len=9
fprintf('--- fspecial(''motion'', 9, 0) ---\n');
fprintf('size=%dx%d sum=%.8f m(1,1)=%.8f  (expect 1x9, 1, 0.11111111)\n', ...
        size(m0,1), size(m0,2), sum(m0(:)), m0(1,1));

m90 = fspecial('motion', 9, 90);          % vertical blur
fprintf('--- fspecial(''motion'', 9, 90) ---\n');
fprintf('size=%dx%d sum=%.8f  (expect 9x1, 1)\n', size(m90,1), size(m90,2), sum(m90(:)));

m45 = fspecial('motion', 9, 45);          % diagonal blur (anti-aliased)
fprintf('--- fspecial(''motion'', 9, 45) ---\n');
fprintf('size=%dx%d sum=%.8f centre=%.8f end=%.8f corner=%.8f\n', ...
        size(m45,1), size(m45,2), sum(m45(:)), m45(4,4), m45(1,7), m45(1,1));
fprintf('  (expect 7x7, 1, 0.09970649, 0.07551364, 0)\n');

m5 = fspecial('motion', 5, 45);
fprintf('--- fspecial(''motion'', 5, 45) ---\n');
fprintf('size=%dx%d centre=%.8f  (expect 5x5, 0.17714908)\n', ...
        size(m5,1), size(m5,2), m5(3,3));

% Default theta = 0 when only len is given.
md = fspecial('motion', 7);
fprintf('--- fspecial(''motion'', 7) default theta=0 ---\n');
fprintf('size=%dx%d sum=%.8f  (expect 1x7, 1)\n', size(md,1), size(md,2), sum(md(:)));
