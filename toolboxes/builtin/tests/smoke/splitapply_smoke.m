clear

% splitapply — apply scalar-returning function per group.
% Reference: MATLAB R2025b.

g = [1 2 1 2 1];
x = [10 20 30 40 50];

fprintf('sum: %s (e [90;60])\n', mat2str(splitapply(@sum, x, g)));
fprintf('mean: %s (e [30;30])\n', mat2str(splitapply(@mean, x, g)));
fprintf('max: %s (e [50;40])\n', mat2str(splitapply(@max, x, g)));
fprintf('min: %s (e [10;20])\n', mat2str(splitapply(@min, x, g)));

% Multi-input handle
y = [100 200 300 400 500];
fprintf('two inputs (sum+sum): %s (e [990;660])\n', ...
    mat2str(splitapply(@(a,b) sum(a)+sum(b), x, y, g)));
