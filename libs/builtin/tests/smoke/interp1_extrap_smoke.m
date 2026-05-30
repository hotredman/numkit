clear

import compat.*

% interp1 out-of-range / extrapolation policy. Bug fixed 2026-05-30:
% 'linear'/'nearest' used to extrapolate by default instead of returning
% NaN (a silent wrong-answer bug); the 'extrap' option worked only by
% accident and a numeric extrapval was unsupported. vs MATLAB R2025b.

format long
x = [1 2 3]; y = [10 20 30];

fprintf('=== default: linear/nearest -> NaN out of range ===\n');
fprintf('interp1(x,y,4)          = %g (expect NaN)\n', interp1(x,y,4));
fprintf('interp1(x,y,0)          = %g (expect NaN)\n', interp1(x,y,0));
fprintf('interp1(x,y,4,nearest)  = %g (expect NaN)\n', interp1(x,y,4,'nearest'));
fprintf('interp1(x,y,2.5)        = %g (in-range, expect 25)\n', interp1(x,y,2.5));

fprintf('\n=== ''extrap'' restores method extrapolation ===\n');
fprintf('linear  extrap 4 / 0    = %g / %g (expect 40 / 0)\n', ...
        interp1(x,y,4,'linear','extrap'), interp1(x,y,0,'linear','extrap'));
fprintf('nearest extrap 4 / 0    = %g / %g (expect 30 / 10)\n', ...
        interp1(x,y,4,'nearest','extrap'), interp1(x,y,0,'nearest','extrap'));
fprintf('previous extrap 4 / 0   = %g / %g (expect 30 / NaN)\n', ...
        interp1(x,y,4,'previous','extrap'), interp1(x,y,0,'previous','extrap'));
fprintf('next     extrap 0 / 4   = %g / %g (expect 10 / NaN)\n', ...
        interp1(x,y,0,'next','extrap'), interp1(x,y,4,'next','extrap'));

fprintf('\n=== numeric extrapval fills out-of-range ===\n');
fprintf('interp1(x,y,[0 2.5 4],linear,-99) = %s (expect [-99 25 -99])\n', ...
        mat2str(interp1(x,y,[0 2.5 4],'linear',-99)));

fprintf('\n=== spline/pchip/makima extrapolate by default ===\n');
fprintf('spline/pchip/makima @4  = %g / %g / %g (expect 40 each)\n', ...
        interp1(x,y,4,'spline'), interp1(x,y,4,'pchip'), interp1(x,y,4,'makima'));
fprintf('spline extrapval -1     = %s (expect [-1 -1])\n', ...
        mat2str(interp1(x,y,[0 4],'spline',-1)));
