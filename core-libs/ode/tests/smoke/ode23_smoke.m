clear;
import compat.*;

% ─── ode23 smoke (Bogacki-Shampine 3(2) with cubic Hermite dense) ──

% 1) Scalar exponential decay: y' = -y, y(0) = 1, t in [0 2].
%    Analytical: y(t) = exp(-t)  ⇒ y(2) ≈ 0.13533528.
fprintf('--- 1) scalar exp decay ---\n');
[t1, y1] = ode23(@(t,y) -y, [0 2], 1);
fprintf('n=%d t(end)=%.6f y(end)=%.10e (expect %.10e)\n', ...
        length(t1), t1(end), y1(end), exp(-2));

% 2) Harmonic oscillator: y(0)=[1;0], t in [0, pi]. Analytical [-1; 0].
fprintf('\n--- 2) harmonic oscillator (2D) ---\n');
[t2, y2] = ode23(@(t,y) [y(2); -y(1)], [0 pi], [1; 0]);
fprintf('n=%d  y1(end)=%.4f (expect -1) y2(end)=%.3e (expect 0)\n', ...
        length(t2), y2(end,1), y2(end,2));

% 3) Explicit tspan + tight tolerance.
fprintf('\n--- 3) explicit tspan, RelTol=1e-9 ---\n');
opts = odeset('RelTol', 1e-9, 'AbsTol', 1e-12);
ts = linspace(0, 1, 6);
[t3, y3] = ode23(@(t,y) -y, ts, 1, opts);
for k = 1:length(t3)
    fprintf('  t=%.4f  y=%.12e (expect %.12e)\n', t3(k), y3(k), exp(-t3(k)));
end

% 4) Reverse integration: tspan = [0 -1].
fprintf('\n--- 4) reverse integration ---\n');
[t4, y4] = ode23(@(t,y) -y, [0 -1], 1);
fprintf('y(end)=%.10e (expect exp(1)=%.10e)\n', y4(end), exp(1));

% 5) MaxStep + default Refine=1 vs Refine=4 override.
fprintf('\n--- 5) Refine override ---\n');
opts1 = odeset('Refine', 1);
[t5, y5] = ode23(@(t,y) -y, [0 2], 1, opts1);
fprintf('Refine=1: n=%d\n', length(t5));
opts4 = odeset('Refine', 4);
[t6, y6] = ode23(@(t,y) -y, [0 2], 1, opts4);
fprintf('Refine=4: n=%d (expect ~4x)\n', length(t6));
