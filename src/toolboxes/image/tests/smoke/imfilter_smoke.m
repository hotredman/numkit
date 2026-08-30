clear

A = [1 2 3; 4 5 6; 7 8 9];

% padarray default mode = constant 0
fprintf('--- padarray(A, [1 1]) constant 0 ---\n');
disp(padarray(A, [1 1]));
fprintf('  expect 5×5 with original A in centre, zeros around\n\n');

% padarray replicate
fprintf('--- padarray(A, [1 1], ''replicate'') ---\n');
disp(padarray(A, [1 1], 'replicate'));
fprintf('  expect: edge replicated\n\n');

% padarray symmetric
fprintf('--- padarray(A, [1 1], ''symmetric'') ---\n');
disp(padarray(A, [1 1], 'symmetric'));
fprintf('  expect: mirrored, with edge included\n\n');

% padarray circular
fprintf('--- padarray(A, [1 1], ''circular'') ---\n');
disp(padarray(A, [1 1], 'circular'));
fprintf('  expect: wrapped\n\n');

% padarray pre/post directions
fprintf('--- padarray(A, [1 1], 0, ''pre'') ---\n');
disp(padarray(A, [1 1], 0, 'pre'));
fprintf('  expect: only top+left padded\n\n');

% fspecial average 3x3
fprintf('--- fspecial(''average'', 3) ---\n');
disp(fspecial('average', 3));
fprintf('  expect: 3×3 of 1/9\n\n');

% fspecial gaussian 5x5 sigma=1
fprintf('--- fspecial(''gaussian'', 5, 1) ---\n');
disp(fspecial('gaussian', 5, 1));
fprintf('  expect: normalised gaussian, peak ≈ 0.1591\n\n');

% fspecial sobel
fprintf('--- fspecial(''sobel'') ---\n');
disp(fspecial('sobel'));
fprintf('  expect: [1 2 1; 0 0 0; -1 -2 -1]\n\n');

% fspecial prewitt
fprintf('--- fspecial(''prewitt'') ---\n');
disp(fspecial('prewitt'));
fprintf('  expect: [1 1 1; 0 0 0; -1 -1 -1]\n\n');

% fspecial laplacian (default alpha)
fprintf('--- fspecial(''laplacian'') ---\n');
disp(fspecial('laplacian'));
fprintf('  expect (alpha=0.2): rough [.167 .667 .167; ...; -3.333 in middle]\n\n');

% fspecial disk r=2
fprintf('--- fspecial(''disk'', 2) ---\n');
disp(fspecial('disk', 2));
fprintf('  expect: 5×5 disk-like radial weights\n');
