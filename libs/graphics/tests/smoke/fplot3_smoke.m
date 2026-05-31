clear
import compat.*

fprintf('=== fplot3 — parametric 3-D curve from function handles ===\n');

% Default range t in [-5, 5].
fprintf('\nfplot3(@(t) cos(t), @(t) sin(t), @(t) t)   default range\n');
fplot3(@(t) cos(t), @(t) sin(t), @(t) t);

% Explicit range — helical spiral.
fprintf('\nfplot3(..., [0 4*pi])   helical spiral, 2 turns\n');
fplot3(@(t) cos(t), @(t) sin(t), @(t) t / (2*pi), [0 4*pi]);

% Lissajous knot.
fprintf('\nLissajous knot: x=sin(3t), y=sin(2t), z=cos(t) on [0,2pi]\n');
fplot3(@(t) sin(3*t), @(t) sin(2*t), @(t) cos(t), [0 2*pi]);

% Function that throws at t=0 — point becomes NaN, plot continues.
fprintf('\nfplot3 with 1/t — should handle singularity gracefully\n');
fplot3(@(t) 1./t, @(t) t, @(t) t);

fprintf('\nAll calls completed without error.\n');
