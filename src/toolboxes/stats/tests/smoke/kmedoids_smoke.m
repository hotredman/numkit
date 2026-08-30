clear;

% kmedoids — k-medoids clustering. Closes
%
%
% Bug fixes 2026-05-08:
%   - default 'Distance' was 'euclidean', MATLAB R2025b uses
%     'sqeuclidean'
%   - 4-output (D), 5-output (midx), 6-output (info) forms missing
%   - 'Algorithm' / 'Start' N-V silently dropped
%   - N-V keys were case-sensitive
%
% Plus PMR refactor of kmedoids.cpp — kmedoids() and dbscan() now use
% ScratchArena + ScratchVec for all scratch buffers.

X = [0 0; 0.1 0; 0 0.1; 0.1 0.1; ...
     5 5; 5.1 5; 5 5.1; 5.1 5.1; ...
     10 0; 10.1 0; 10 0.1; 10.1 0.1];

fprintf('--- 2-output (default sqeuclidean) ---\n');
[idx, C] = kmedoids(X, 3);
fprintf('idx = '); disp(idx');
fprintf('C =\n'); disp(C);

fprintf('--- 6-output (D + midx + info struct) ---\n');
[idx, C, sumd, D, midx, info] = kmedoids(X, 3);
fprintf('size(D) = [%d %d] (expect [12 3])\n', size(D, 1), size(D, 2));
fprintf('midx = '); disp(midx');
fprintf('info.algorithm = %s\n', info.algorithm);
fprintf('info.distance = %s (expect sqeuclidean)\n', info.distance);
fprintf('info.iterations = %d\n', info.iterations);

fprintf('--- mixed-case N-V + Algorithm + Start ---\n');
idx2 = kmedoids(X, 3, 'algorithm', 'pam', 'start', 'plus', 'replicates', 2);
fprintf('idx2 = '); disp(idx2');

fprintf('--- explicit Distance cityblock ---\n');
idx3 = kmedoids(X, 3, 'Distance', 'cityblock');
fprintf('idx3 = '); disp(idx3');
fprintf('expect 3 clusters in all cases (label IDs may differ from MATLAB)\n');
