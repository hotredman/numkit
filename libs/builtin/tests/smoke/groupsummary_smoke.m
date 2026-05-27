clear

% groupsummary — array form, multiple reductions, NaN handling.
% Reference: MATLAB R2025b.

A = [10; 20; 30; 40; 50];
G = [1; 2; 1; 2; 1];

fprintf('== sum ==\n');
[B, BG, BC] = groupsummary(A, G, 'sum');
fprintf('  B=%s  BG=%s  BC=%s\n', mat2str(B), mat2str(BG), mat2str(BC));

fprintf('\n== other reductions ==\n');
fprintf('  mean=%s   median=%s\n', mat2str(groupsummary(A, G, 'mean')), mat2str(groupsummary(A, G, 'median')));
fprintf('  max=%s    min=%s\n', mat2str(groupsummary(A, G, 'max')), mat2str(groupsummary(A, G, 'min')));
fprintf('  std=%s    numunique=%s\n', mat2str(groupsummary(A, G, 'std')), mat2str(groupsummary(A, G, 'numunique')));

fprintf('\n== matrix A ==\n');
A2 = [10 100; 20 200; 30 300; 40 400; 50 500];
B = groupsummary(A2, G, 'sum');
fprintf('  size=[%d %d]   B(1,:)=[%g %g]  B(2,:)=[%g %g]\n', size(B,1), size(B,2), B(1,1), B(1,2), B(2,1), B(2,2));

fprintf('\n== NaN group (trailing bucket) ==\n');
G2 = [1; 2; NaN; 1; 2];
B = groupsummary(A, G2, 'sum');
fprintf('  B=%s (e [50;70;30])\n', mat2str(B));
