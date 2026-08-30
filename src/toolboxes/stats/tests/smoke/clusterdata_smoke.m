clear;

% clusterdata — convenience wrapper: pdist + linkage + cluster.
%
%
% Bug fixes 2026-05-08 (initial review said "no major gap" but re-probe
% surfaced 4 real bugs):
%   1. scalar c < 2 was interpreted as maxclust (gave 0 → all
%      singletons); MATLAB uses cutoff (inconsistency)
%   2. N-V keys were case-sensitive ('Cutoff' / 'Linkage' silently
%      dropped)
%   3. 'Distance' N-V was unwired (hardcoded euclidean)
%   4. 'Depth' N-V was unwired
%
% Plus PMR refactor of linkage.cpp — all scratch buffers now go via
% ScratchArena + ScratchVec.

X = [1 1; 1.5 1.5; 5 5; 5.5 5.5; 10 10; 1 2; 6 6; 11 11];

fprintf('--- clusterdata(X, 0.5) [scalar shortcut: 0<c<2 → cutoff] ---\n');
disp(clusterdata(X, 0.5)');
fprintf('expect 3 clusters (root inc 0.7259 > 0.5)\n\n');

fprintf('--- clusterdata(X, 3) [scalar shortcut: c>=2 → maxclust] ---\n');
disp(clusterdata(X, 3)');
fprintf('expect 3 clusters\n\n');

fprintf('--- clusterdata(X, 1.5) [cutoff > root inc → 1 cluster] ---\n');
disp(clusterdata(X, 1.5)');
fprintf('expect 1 cluster\n\n');

fprintf('--- clusterdata(X, ''MaxClust'', 3, ''Linkage'', ''ward'') [mixed case] ---\n');
disp(clusterdata(X, 'MaxClust', 3, 'Linkage', 'ward')');
fprintf('expect 3 clusters\n\n');

fprintf('--- clusterdata(X, ''Cutoff'', 1.0, ''Criterion'', ''distance'') ---\n');
disp(clusterdata(X, 'Cutoff', 1.0, 'Criterion', 'distance')');
fprintf('expect 4 clusters\n\n');

fprintf('--- clusterdata(X, ''maxclust'', 3, ''Distance'', ''cityblock'') ---\n');
disp(clusterdata(X, 'maxclust', 3, 'Distance', 'cityblock')');
fprintf('expect 3 clusters\n');
