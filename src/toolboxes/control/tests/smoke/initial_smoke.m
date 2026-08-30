clear

% Initial-condition response: zero-input simulation from x0, y = C*x.
% bugs/control/initial. Same integrator as step/impulse with u==0, x(0)=x0.

% First-order G with A=-2, x0=1 -> y = e^{-2t}. Explicit grid = exact.
[y, t] = initial(ss(-2, 0, 1, 0), 1, 0:0.1:3);
fprintf('initial 1st-order: y(1)=%.8f  y(end)=%.12f  (expect 1, e^-6=0.002478752177)\n', ...
        y(1), y(end));

% Auto grid (no t): matches MATLAB horizon closely (heuristic).
[ya, ta] = initial(ss(-2, 0, 1, 0), 1);
fprintf('initial auto: y(end)=%.10f numel=%d  (expect ~0.0030199517, ~127)\n', ...
        ya(end), numel(ta));

% Two-state x0=[1;0], poles -1,-2: y(t=1) = first row of expm(A)*x0.
[y2, t2, x2] = initial(ss([0 1; -2 -3], [0; 0], [1 0], 0), [1; 0], 0:0.05:5);
fprintf('initial 2-state: y(t=1)=%.10f  y(end)=%.12f  (expect 0.6004235991, 0.013430494)\n', ...
        y2(21), y2(end));
fprintf('  state x(0) = [%.4f %.4f]  (expect [1 0])\n', x2(1,1), x2(1,2));
