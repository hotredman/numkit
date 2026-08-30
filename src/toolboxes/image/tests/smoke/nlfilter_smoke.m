clear;

A = magic(5);
fprintf('Input A = magic(5):\n');
disp(A);

fprintf('\n--- (1) mean over 3x3 (≡ imboxfilt with norm) ---\n');
B = nlfilter(A, [3 3], @(x) mean(x(:)));
disp(B);

fprintf('--- (2) max over 3x3 (≡ imdilate with ones(3)) ---\n');
B = nlfilter(A, [3 3], @(x) max(x(:)));
disp(B);

fprintf('--- (3) median over 3x3 (≡ medfilt2) ---\n');
B = nlfilter(A, [3 3], @(x) median(x(:)));
disp(B);

fprintf('--- (4) sum over even 2x3 window ---\n');
B = nlfilter(A, [2 3], @(x) sum(x(:)));
disp(B);

fprintf('--- (5) uint8 input — output class follows fun() ---\n');
A8 = uint8(A);
B = nlfilter(A8, [3 3], @(x) uint8(mean(x(:))));
fprintf('class(B) = %s\n', class(B));
for r = 1:5
    fprintf('  '); for c = 1:5; fprintf('%4u', B(r,c)); end; fprintf('\n');
end

fprintf('--- (6) ''indexed'' mode: padval = 1 for double ---\n');
B = nlfilter(A, 'indexed', [3 3], @(x) min(x(:)));
disp(B);
