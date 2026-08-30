clear

% integralImage / integralImage3 — summed-area / summed-volume tables.

% --- 2-D ---
fprintf('--- integralImage(I) ---\n');
I  = [16 2 3 13; 5 11 10 8; 9 7 6 12; 4 14 15 1];
J  = integralImage(I);
fprintf('size(I) = %s, size(J) = %s\n', mat2str(size(I)), mat2str(size(J)));
disp(J);
fprintf('  expect: 5x5, top row + left col are 0; J(end,end) = sum(I(:)) = %d\n\n', ...
        sum(I(:)));   % 136

% --- rectangle sum via integral image ---
fprintf('--- rect sum [r=2..3, c=2..3] of magic(4) ---\n');
r0 = 2; r1 = 3; c0 = 2; c1 = 3;
direct = sum(sum(I(r0:r1, c0:c1)));
viaInt = J(r1+1, c1+1) - J(r0, c1+1) - J(r1+1, c0) + J(r0, c0);
fprintf('direct = %d, via integral = %d\n', direct, viaInt);
fprintf('  expect: equal\n\n');

% --- 3-D ---
fprintf('--- integralImage3(V) ---\n');
V  = reshape(1:24, [2 3 4]);
K  = integralImage3(V);
fprintf('size(V) = %s, size(K) = %s\n', mat2str(size(V)), mat2str(size(K)));
fprintf('K(end,end,end) = %g, sum(V(:)) = %g\n', K(end, end, end), sum(V(:)));
fprintf('  expect: 3x4x5; K(end,end,end) = sum(V(:)) = 300\n');
