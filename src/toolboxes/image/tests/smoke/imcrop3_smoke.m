clear;

V = reshape(1:60, 3, 4, 5);
fprintf('--- (1) basic [1 1 1 2 1 2] crop ---\n');
W = imcrop3(V, [1 1 1 2 1 2]);
fprintf('  size = [%d %d %d]\n', size(W,1), size(W,2), size(W,3));
for p = 1:size(W,3)
    fprintf('  page %d:\n', p);
    disp(W(:,:,p));
end

fprintf('--- (2) interior crop [2 2 1 2 1 2] ---\n');
W = imcrop3(V, [2 2 1 2 1 2]);
fprintf('  size = [%d %d %d]\n', size(W,1), size(W,2), size(W,3));
for p = 1:size(W,3)
    fprintf('  page %d:\n', p);
    disp(W(:,:,p));
end

fprintf('--- (3) uint8 4-D input: class-preserving + 4th dim pass-through ---\n');
V4 = uint8(reshape(1:96, 4, 4, 3, 2));
W4 = imcrop3(V4, [1 1 1 2 2 0]);
fprintf('  size = [%d %d %d %d]\n', size(W4,1), size(W4,2), size(W4,3), size(W4,4));
fprintf('  W4(:,:,1,1):\n');
for r = 1:3
    fprintf('  '); for c = 1:3; fprintf(' %3u', W4(r,c,1,1)); end; fprintf('\n');
end
fprintf('  W4(:,:,1,2):\n');
for r = 1:3
    fprintf('  '); for c = 1:3; fprintf(' %3u', W4(r,c,1,2)); end; fprintf('\n');
end

fprintf('--- (4) non-integer rounding [1.6 1.6 1.6 1.4 1.4 1.4] ---\n');
W = imcrop3(V, [1.6 1.6 1.6 1.4 1.4 1.4]);
fprintf('  size = [%d %d %d]\n', size(W,1), size(W,2), size(W,3));
for p = 1:size(W,3)
    fprintf('  page %d:\n', p); disp(W(:,:,p));
end

fprintf('--- (5) out-of-bounds throws ---\n');
try
    imcrop3(V, [10 10 10 1 1 1]);
    fprintf('  NO ERROR (unexpected)\n');
catch e
    fprintf('  ERROR (expected): %s\n', e.message);
end
