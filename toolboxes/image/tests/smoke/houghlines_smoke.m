clear
import compat.*

% houghlines — line-segment extraction from Hough-transform peaks.
% Bit-exact MATLAB R2025b.

BW = false(11, 11);
BW(6, 1:11) = true;
BW(1:11, 6) = true;
[H, T, R] = hough(BW);
P = houghpeaks(H, 2);

fprintf('=== custom FillGap=5, MinLength=3 ===\n');
lines = houghlines(BW, T, R, P, 'FillGap', 5, 'MinLength', 3);
fprintf('%d segments (expect 2)\n', length(lines));
for k = 1:length(lines)
    fprintf('  L(%d): p1=[%g %g] p2=[%g %g] theta=%g rho=%g\n', ...
        k, lines(k).point1(1), lines(k).point1(2), ...
        lines(k).point2(1), lines(k).point2(2), ...
        lines(k).theta, lines(k).rho);
end

fprintf('\n=== default fillgap=20, minlength=40 ===\n');
lines2 = houghlines(BW, T, R, P);
fprintf('%d segments (expect 0 — image too small)\n', length(lines2));

fprintf('\n=== single peak ===\n');
P1 = houghpeaks(H, 1);
lines3 = houghlines(BW, T, R, P1, 'MinLength', 3);
fprintf('%d segments (expect 1)\n', length(lines3));
