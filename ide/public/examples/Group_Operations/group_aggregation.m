% findgroups / splitapply / groupcounts — Per-group Aggregation
%
% MATLAB's lightweight alternative to GROUP BY clauses. Useful for
% summarising experimental data without pulling in a table type.

import compat.*;

clear
close all

% Synthetic dataset: 3 experiment groups × varying sample counts.
g = [1 1 1 1 2 2 2 3 3 3 3 3];          % group ID per measurement
x = [10 12 11 13 25 27 30 5 6 7 8 9];   % measurements

% --- 1. findgroups + ID list ---
[G, ID] = findgroups(g);
fprintf('findgroups: %d unique groups, IDs = ', length(ID));
fprintf('%d ', ID); fprintf('\n');

% --- 2. groupcounts ---
counts = groupcounts(g);
fprintf('groupcounts: ');
for i = 1:length(counts)
    fprintf('group %d → %d samples; ', ID(i), counts(i));
end
fprintf('\n');

% --- 3. splitapply with @mean ---
means = splitapply(@mean, x, g);
fprintf('splitapply(@mean):  ');
for i = 1:length(means)
    fprintf('group %d → mean=%.2f; ', ID(i), means(i));
end
fprintf('\n');

% --- 4. splitapply with @max ---
maxes = splitapply(@max, x, g);
fprintf('splitapply(@max):   ');
for i = 1:length(maxes)
    fprintf('group %d → max=%g; ', ID(i), maxes(i));
end
fprintf('\n');

% --- 5. Visualise group statistics as a grouped bar chart ---
% Rows = groups; columns = mean / max / count.
stats = [means'; maxes'; counts'];
figure;
bar(ID, stats');
title('Per-group statistics');
xlabel('group ID'); ylabel('value');
legend('mean', 'max', 'count');
