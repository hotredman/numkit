clear;

I = double([1 2 3; 2 3 4; 3 4 5]);

fprintf('--- (1) refGrayVal scalar = 3 ---\n');
W = graydiffweight(I, 3.0);
for r = 1:3; fprintf('  '); for c = 1:3; fprintf('%.3e  ', W(r,c)); end; fprintf('\n'); end

fprintf('\n--- (2) RolloffFactor = 2 ---\n');
W = graydiffweight(I, 3.0, 'RolloffFactor', 2);
for r = 1:3; fprintf('  '); for c = 1:3; fprintf('%.3e  ', W(r,c)); end; fprintf('\n'); end

fprintf('\n--- (3) MASK form (mean(I(M)) = 3) ---\n');
M = false(size(I)); M(1,1)=true; M(3,3)=true;
W = graydiffweight(I, M);
for r = 1:3; fprintf('  '); for c = 1:3; fprintf('%.3e  ', W(r,c)); end; fprintf('\n'); end

fprintf('\n--- (4) (C, R) form ---\n');
W = graydiffweight(I, [1;3], [1;3]);
for r = 1:3; fprintf('  '); for c = 1:3; fprintf('%.3e  ', W(r,c)); end; fprintf('\n'); end

fprintf('\n--- (5) 3-D volume ---\n');
V = double(reshape(1:24, 2, 3, 4));
W = graydiffweight(V, 12.0);
fprintf('  size W = [%d %d %d]\n', size(W,1), size(W,2), size(W,3));
fprintf('  W(:,:,2):\n');
for r = 1:2; fprintf('  '); for c = 1:3; fprintf('%.3e ', W(r,c,2)); end; fprintf('\n'); end

fprintf('\n--- (6) Cutoff = 1 ---\n');
W = graydiffweight(I, 3.0, 'GrayDifferenceCutoff', 1);
for r = 1:3; fprintf('  '); for c = 1:3; fprintf('%.3e  ', W(r,c)); end; fprintf('\n'); end
