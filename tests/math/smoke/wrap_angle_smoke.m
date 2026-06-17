clear

import compat.*

% Angle-wrapping family: wrapToPi / wrapTo2Pi / wrapTo180 / wrapTo360.
% New 2026-05-30 — completes the angle group alongside deg2rad/rad2deg.
% Matches MATLAB R2025b Mapping-Toolbox definitions, including boundary
% quirks. vs MATLAB R2025b at format long.

format long

fprintf('=== wrapToPi -> [-pi, pi] ===\n');
fprintf('wrapToPi([4 -4 7]) = %s (expect [-2.28319 2.28319 0.716815])\n', ...
        mat2str(wrapToPi([4 -4 7]), 6));
fprintf('wrapToPi([pi -pi]) = %s (closed endpoints kept: [pi -pi])\n', ...
        mat2str(wrapToPi([pi -pi]), 6));

fprintf('\n=== wrapTo2Pi -> [0, 2*pi] ===\n');
fprintf('wrapTo2Pi([-1 7 -7]) = %s (expect [5.28319 0.716815 5.56637])\n', ...
        mat2str(wrapTo2Pi([-1 7 -7]), 6));
fprintf('wrapTo2Pi([2*pi 0 -2*pi]) = %s (positive->2pi, else 0: [6.28319 0 0])\n', ...
        mat2str(wrapTo2Pi([2*pi 0 -2*pi]), 6));

fprintf('\n=== wrapTo180 -> [-180, 180] ===\n');
fprintf('wrapTo180([190 -190 540]) = %s (expect [-170 170 180])\n', ...
        mat2str(wrapTo180([190 -190 540])));
fprintf('wrapTo180([180 -180]) = %s (closed endpoints kept: [180 -180])\n', ...
        mat2str(wrapTo180([180 -180])));

fprintf('\n=== wrapTo360 -> [0, 360] ===\n');
fprintf('wrapTo360([-10 370 720]) = %s (expect [350 10 360])\n', ...
        mat2str(wrapTo360([-10 370 720])));
fprintf('wrapTo360([360 0]) = %s (positive 360->360, 0->0: [360 0])\n', ...
        mat2str(wrapTo360([360 0])));
