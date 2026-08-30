clear

% H-infinity norm via the Bruinsma-Steinbuch Hamiltonian bisection.
% bugs/control/hinfnorm. ||G||inf = sup_w sigma_max(G(jw)); Inf if a pole
% sits on/right of the jw axis. Reference values from MATLAB R2025b.

% 1/(s+1): peak |G| at w=0 is 1.
g1 = hinfnorm(ss(-1, 1, 1, 0));
fprintf('hinfnorm 1/(s+1)        = %.8f   (expect 1)\n', g1);

% poles at +-i (on jw axis) -> Inf.
g2 = hinfnorm(ss([0 1; -1 0], [0; 1], [1 0], 0));
fprintf('hinfnorm marginal (+-i) = %g   (expect Inf)\n', g2);

% lightly-damped resonance 1/(s^2+0.1s+1): peak ~10 near w=1.
g3 = hinfnorm(ss([0 1; -1 -0.1], [0; 1], [1 0], 0));
fprintf('hinfnorm resonance      = %.6f   (expect 10.012523)\n', g3);

% sum of two real poles, static peak at w=0 = 1/2+1/3 = 0.83333.
g4 = hinfnorm(ss([-2 0; 0 -3], [1; 1], [1 1], 0));
fprintf('hinfnorm static peak    = %.8f   (expect 0.83333333)\n', g4);

% D != 0: 0.5 + 1/(s+1) -> 1.5 at w=0.
g5 = hinfnorm(ss(-1, 1, 1, 0.5));
fprintf('hinfnorm with D=0.5     = %.8f   (expect 1.5)\n', g5);

% tf input path
g6 = hinfnorm(tf(1, [1 2 1]));   % 1/(s+1)^2 -> 1 at w=0
fprintf('hinfnorm tf 1/(s+1)^2   = %.8f   (expect 1)\n', g6);
