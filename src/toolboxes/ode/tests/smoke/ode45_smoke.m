clear;

% ─── ode45 smoke (Dormand-Prince 5(4) with Shampine dense output) ──

% 1) Scalar exponential decay: y' = -y, y(0) = 1, t in [0 2].
%    Analytical: y(t) = exp(-t)  ⇒ y(2) ≈ 0.13533528.
fprintf('--- 1) scalar exp decay ---\n');
[t1, y1] = ode45(@(t,y) -y, [0 2], 1);
fprintf('t(end)=%.6f y(end)=%.10e   (expect ~exp(-2)=%.10e)\n', ...
        t1(end), y1(end), exp(-2));

% 2) Harmonic oscillator: y1' = y2, y2' = -y1; y(0) = [1; 0]; t in [0 pi].
%    Analytical: y(t) = [cos(t); -sin(t)]  ⇒ y(pi) ≈ [-1; 0].
fprintf('\n--- 2) harmonic oscillator (2D) ---\n');
[t2, y2] = ode45(@(t,y) [y(2); -y(1)], [0 pi], [1; 0]);
fprintf('size(y)=[%d %d]  y1(end)=%.6f (expect -1)  y2(end)=%.4e (expect 0)\n', ...
        size(y2,1), size(y2,2), y2(end,1), y2(end,2));

% 3) Explicit tspan + tight tolerance.
fprintf('\n--- 3) explicit tspan, RelTol=1e-9 ---\n');
opts = odeset('RelTol', 1e-9, 'AbsTol', 1e-12);
ts = linspace(0, 1, 6);
[t3, y3] = ode45(@(t,y) -y, ts, 1, opts);
for k = 1:length(t3)
    fprintf('  t=%.4f  y=%.12e  (expect=%.12e)\n', t3(k), y3(k), exp(-t3(k)));
end

% 4) Reverse integration: tspan = [0 -1], y' = -y, y(0) = 1.
%    Analytical: y(-1) = exp(1).
fprintf('\n--- 4) reverse integration ---\n');
[t4, y4] = ode45(@(t,y) -y, [0 -1], 1);
fprintf('t(end)=%.4f y(end)=%.10e (expect exp(1)=%.10e)\n', ...
        t4(end), y4(end), exp(1));

% 5) MaxStep enforcement.
fprintf('\n--- 5) MaxStep=0.1, [0 2] ---\n');
optsM = odeset('MaxStep', 0.1);
[t5, y5] = ode45(@(t,y) -y, [0 2], 1, optsM);
fprintf('n=%d (expect ~81 with Refine=4)  y(end)=%.10e\n', ...
        length(t5), y5(end));

% 6) Refine option (default 4, override 1).
fprintf('\n--- 6) Refine=1 vs default Refine=4 ---\n');
opts1 = odeset('Refine', 1);
[t6, y6] = ode45(@(t,y) -y, [0 2], 1, opts1);
fprintf('Refine=1: n=%d\n', length(t6));
[t7, y7] = ode45(@(t,y) -y, [0 2], 1);
fprintf('Refine=4: n=%d (expect ~4x)\n', length(t7));
