clear

% topkrows — extended forms: col, direction, 2-output index.
% Reference: MATLAB R2025b.

A = [3 1; 1 2; 4 5; 4 3; 2 0];

fprintf('== col 1 desc (default) ==\n');
B = topkrows(A, 2, 1);
fprintf('  [%g %g; %g %g] (e [4 5; 4 3])\n', B(1,1), B(1,2), B(2,1), B(2,2));

fprintf('\n== cols [2 1] desc, top 3 ==\n');
B = topkrows(A, 3, [2 1]);
disp(B);

fprintf('\n== col 1 ascend ==\n');
B = topkrows(A, 2, 1, 'ascend');
fprintf('  [%g %g; %g %g] (e [1 2; 2 0])\n', B(1,1), B(1,2), B(2,1), B(2,2));

fprintf('\n== with index ==\n');
[B, I] = topkrows(A, 2);
fprintf('  I = [%d; %d] (e [3; 4])\n', I(1), I(2));

fprintf('\n== ComparisonMethod NV (accept-and-ignore) ==\n');
B = topkrows(A, 2, 1, 'ComparisonMethod', 'auto');
fprintf('  [%g %g] (e [4 5])\n', B(1,1), B(1,2));
