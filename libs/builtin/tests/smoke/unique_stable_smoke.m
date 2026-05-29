clear
import compat.*

% unique 'stable' setOrder: keep first-occurrence order, no sort.
x = [3 1 4 1 5 9 2 6 5 3];

u_sorted = unique(x);
fprintf('sorted : %s\n', mat2str(u_sorted));      % expect [1 2 3 4 5 6 9]

u_stable = unique(x, 'stable');
fprintf('stable : %s\n', mat2str(u_stable));       % expect [3 1 4 5 9 2 6]

% Three-output stable form.
[u, ia, ic] = unique([3 1 4 1 5], 'stable');
fprintf('u  : %s\n', mat2str(u));                  % expect [3 1 4 5]
fprintf('ia : %s\n', mat2str(ia(:).'));            % expect [1 2 3 5]
fprintf('ic : %s\n', mat2str(ic(:).'));            % expect [1 2 3 2 4]

% Reconstruction checks: u == x(ia) and x == u(ic).
xx = [3 1 4 1 5];
fprintf('x(ia) ok : %d\n', isequal(u(:).', xx(ia)));   % expect 1
fprintf('u(ic) ok : %d\n', isequal(u(ic), xx(:).'));   % expect 1

% 'first'/'last' keep sorted order (occurrence selector only).
fprintf('first  : %s\n', mat2str(unique([2 2 1 1], 'first')));  % expect [1 2]
