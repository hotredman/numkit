clear

import compat.*

% interp1 string ("double-quoted") method/extrap args (DEEP-PROBE 2026-05-31).
% MATLAB accepts BOTH char ('linear') and string ("linear") for the method
% and the extrapolation option. numkit honored only char and SILENTLY
% IGNORED a double-quoted method, falling back to linear. vs MATLAB R2025b.

x = [1 2 3]; y = [10 20 30];

fprintf('=== method as string vs char ===\n');
fprintf('interp1(...,1.4,"nearest") = %g  (expect 10, NOT linear 14)\n', interp1(x,y,1.4,"nearest"));
fprintf('interp1(...,1.4,''nearest'') = %g  (expect 10)\n', interp1(x,y,1.4,'nearest'));

fprintf('\n=== extrap option as string ===\n');
fprintf('interp1(...,4,"linear","extrap") = %g  (expect 40)\n', interp1(x,y,4,"linear","extrap"));
fprintf('interp1(...,4,"spline","extrap") = %g  (expect 40)\n', interp1(x,y,4,"spline","extrap"));
fprintf('interp1(...,4,"linear",99)       = %g  (expect 99, scalar fill)\n', interp1(x,y,4,"linear",99));

fprintf('\n=== default (no extrap) still NaN out of range ===\n');
fprintf('interp1(...,4,"linear") = %g  (expect NaN)\n', interp1(x,y,4,"linear"));
