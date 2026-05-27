clear

% grouptransform — array form, per-group transformations.
% Reference: MATLAB R2025b.

A = [10; 20; 30; 40; 50]; G = [1; 2; 1; 2; 1];

fprintf('== meancenter ==\n');
fprintf('  %s (e [-20;-10;0;10;20])\n', mat2str(grouptransform(A, G, 'meancenter')));

fprintf('\n== zscore ==\n');
fprintf('  %s (e [-1;-0.707;0;0.707;1])\n', mat2str(grouptransform(A, G, 'zscore')));

fprintf('\n== rescale ==\n');
fprintf('  %s (e [0;0;0.5;1;1])\n', mat2str(grouptransform(A, G, 'rescale')));

fprintf('\n== norm ==\n');
fprintf('  %s\n', mat2str(grouptransform(A, G, 'norm')));

A3 = [10; NaN; 30; 40; NaN];
fprintf('\n== meanfill ==\n');
fprintf('  %s (e [10;40;30;40;20])\n', mat2str(grouptransform(A3, G, 'meanfill')));

fprintf('\n== linearfill ==\n');
fprintf('  %s (e [10;NaN;30;40;50])\n', mat2str(grouptransform(A3, G, 'linearfill')));

fprintf('\n== function handle ==\n');
fprintf('  %s (e [-20;-10;0;10;20])\n', mat2str(grouptransform(A, G, @(x) x - mean(x))));
