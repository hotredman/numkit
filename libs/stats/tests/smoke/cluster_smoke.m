clear;
import compat.*;

% cluster — flatten a linkage tree into per-sample cluster labels.
% Closes audit/findings/cluster/cluster.md gaps #1-#3:
%   #1 default 'cutoff' criterion is 'inconsistent' (was 'distance');
%   #2 'criterion' N-V parsed;
%   #3 'depth' N-V parsed.

X = [1 1; 1.5 1.5; 5 5; 5.5 5.5; 10 10; 1 2; 6 6; 11 11];
Z = linkage(pdist(X));
fprintf('--- linkage Z ---\n'); disp(Z);
fprintf('--- inconsistent(Z) ---\n'); disp(inconsistent(Z));

fprintf('--- cluster(Z, ''maxclust'', 3) ---\n');
disp(cluster(Z, 'maxclust', 3)');
fprintf('expect 3 clusters: {1,2,6} {3,4,7} {5,8} (label IDs may differ)\n\n');

fprintf('--- cluster(Z, ''cutoff'', 0.5) [INCONSISTENCY default] ---\n');
disp(cluster(Z, 'cutoff', 0.5)');
fprintf('expect 3 clusters (root inc 0.7259 > 0.5)\n\n');

fprintf('--- cluster(Z, ''cutoff'', 5) [inconsistency] ---\n');
disp(cluster(Z, 'cutoff', 5)');
fprintf('expect 1 cluster (root inc 0.7259 < 5)\n\n');

fprintf('--- cluster(Z, ''cutoff'', 0.5, ''criterion'', ''distance'') ---\n');
disp(cluster(Z, 'cutoff', 0.5, 'criterion', 'distance')');
fprintf('expect 8 singleton clusters (every link 0.7071 > 0.5)\n\n');

fprintf('--- cluster(Z, ''cutoff'', 2, ''criterion'', ''distance'') ---\n');
disp(cluster(Z, 'cutoff', 2, 'criterion', 'distance')');
fprintf('expect 3 clusters\n\n');

fprintf('--- cluster(Z, ''cutoff'', 0.5, ''depth'', 4) ---\n');
disp(cluster(Z, 'cutoff', 0.5, 'depth', 4)');
fprintf('expect 3 clusters (depth N-V parsed)\n');
