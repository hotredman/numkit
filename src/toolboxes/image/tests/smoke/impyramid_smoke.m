clear

% impyramid — Burt-Adelson pyramid step (5-tap separable kernel).

% --- reduce: 8x8 → 4x4 ---
fprintf('--- impyramid(I, ''reduce'') ---\n');
I8 = uint8(reshape(0:63, [8 8]));
R  = impyramid(I8, 'reduce');
fprintf('size: %s (expect 4x4)\n', mat2str(size(R)));
disp(double(R));

% --- expand: 4x4 → 7x7 ---
fprintf('--- impyramid(R, ''expand'') ---\n');
E  = impyramid(R, 'expand');
fprintf('size: %s (expect 7x7)\n', mat2str(size(E)));

% --- reduce on uniform image preserves uniform value ---
fprintf('--- reduce(uniform 100) ---\n');
U  = uint8(100 * ones(6, 6));
RU = impyramid(U, 'reduce');
fprintf('reduced size %s, range [%d, %d]\n', mat2str(size(RU)), min(RU(:)), max(RU(:)));
fprintf('  expect: 3x3, all 100\n');

% --- expand on uniform image preserves uniform value ---
fprintf('--- expand(uniform 100) ---\n');
EU = impyramid(U, 'expand');
fprintf('expanded size %s, range [%d, %d]\n', mat2str(size(EU)), min(EU(:)), max(EU(:)));
fprintf('  expect: 11x11, mostly 100\n');

% --- 3-channel RGB ---
fprintf('--- reduce(RGB 6x6x3) ---\n');
RGB = uint8(cat(3, 50*ones(6,6), 100*ones(6,6), 150*ones(6,6)));
RR  = impyramid(RGB, 'reduce');
fprintf('size %s, channel means [%d %d %d]\n', mat2str(size(RR)), ...
        round(mean(reshape(RR(:,:,1),1,[]))), ...
        round(mean(reshape(RR(:,:,2),1,[]))), ...
        round(mean(reshape(RR(:,:,3),1,[]))));
fprintf('  expect: 3x3x3, [50 100 150]\n');
