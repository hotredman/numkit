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

% Complex unique: order by magnitude |z| then phase angle arg(z). MATLAB R2025b.
[cu, cia, cic] = unique([3+4i 1 3+4i 5i]);
fprintf('\ncomplex C  : %s (expect [1 3+4i 5i])\n', mat2str(cu));
fprintf('complex ia : %s (expect [2 1 4])\n', mat2str(cia(:).'));
fprintf('complex ic : %s (expect [2 1 2 3])\n', mat2str(cic(:).'));
fprintf('complex stable : %s (expect [3+4i 1 5i])\n', mat2str(unique([3+4i 1 3+4i 5i], 'stable')));
fprintf('complex ties   : %s (expect [-1i 1i 2 -2])\n', mat2str(unique([2+0i -2 1i -1i 2])));
