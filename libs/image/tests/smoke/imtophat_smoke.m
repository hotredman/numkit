import compat.*

% Bright dot on dark background. Opening with a 3×3 SE removes
% lone bright pixels — so I − imopen leaves the dot's intensity.
I = double([
    0 0 0 0 0;
    0 0 0 0 0;
    0 0 9 0 0;
    0 0 0 0 0;
    0 0 0 0 0]);
SE = strel('square', 3);

T = imtophat(I, SE);
fprintf('--- imtophat(dot, square(3)) ---\n');
fprintf('  T(3,3) = %.1f (expect 9 — lone dot fully extracted)\n', T(3, 3));
fprintf('  sum(T(:)) = %.1f (expect 9 — only the dot remains)\n\n', sum(T(:)));

% Larger bright square (3×3) survives the 3×3 opening, so tophat = 0.
I2 = zeros(7, 7);
I2(3:5, 3:5) = 7;
T2 = imtophat(I2, SE);
fprintf('--- imtophat(3x3 square, square(3)) ---\n');
fprintf('  max(T2(:)) = %.1f (expect 0 — square survives opening)\n\n', ...
    max(T2(:)));

% Bright spike on a smooth background (gradient). Tophat enhances
% the spike, removes the background.
I3 = double([
    1 2 3 4 5 6 7;
    1 2 3 9 5 6 7;
    1 2 3 4 5 6 7]);
T3 = imtophat(I3, strel('square', 5));
fprintf('--- imtophat(spike on gradient, square(5)) ---\n');
fprintf('  T3(2,4) = %.1f (expect > 0 — spike is the only bright detail)\n', ...
    T3(2, 4));
fprintf('  T3(2,1) = %.1f (expect 0 — background suppressed)\n\n', ...
    T3(2, 1));

% --- imbothat: dual on a dark spike ---
% Dark dot (0) on bright background (9). Closing fills the dot in;
% imclose - I leaves the dot's depth.
J = double([
    9 9 9 9 9;
    9 9 9 9 9;
    9 9 0 9 9;
    9 9 9 9 9;
    9 9 9 9 9]);
B = imbothat(J, SE);
fprintf('--- imbothat(dark dot, square(3)) ---\n');
fprintf('  B(3,3) = %.1f (expect 9 — dark dot fully extracted)\n', B(3, 3));
fprintf('  sum(B(:)) = %.1f (expect 9 — only the dot remains)\n\n', sum(B(:)));

% Identity check via duality:
% imbothat(I, SE) ≡ imtophat(complement(I), SE)
Ic = max(I(:)) - I;          % manual complement (single-class)
B_via_dual = imtophat(Ic, SE);
fprintf('--- duality imbothat(I, SE) == imtophat(complement(I), SE) ---\n');
delta = max(max(abs(imbothat(I, SE) - B_via_dual)));
fprintf('  max|Δ| = %.6e (expect 0)\n', delta);
