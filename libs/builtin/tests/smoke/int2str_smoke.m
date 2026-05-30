clear

import compat.*

% int2str: round half away from zero -> plain integer string. Implemented
% 2026-05-30 (was an undefined function). vs MATLAB R2025b.

fprintf('=== rounding (half away from zero) ===\n');
fprintf('int2str(3.4)  = [%s] (expect 3)\n',  int2str(3.4));
fprintf('int2str(2.5)  = [%s] (expect 3)\n',  int2str(2.5));
fprintf('int2str(-2.5) = [%s] (expect -3)\n', int2str(-2.5));
fprintf('int2str(0.5)  = [%s] (expect 1)\n',  int2str(0.5));
fprintf('int2str(-0.5) = [%s] (expect -1)\n', int2str(-0.5));

fprintf('\n=== large value: full integer, no scientific ===\n');
fprintf('int2str(1e10) = [%s] (expect 10000000000)\n', int2str(1e10));

fprintf('\n=== non-finite passthrough ===\n');
fprintf('int2str(Inf)  = [%s] (expect Inf)\n',  int2str(Inf));
fprintf('int2str(-Inf) = [%s] (expect -Inf)\n', int2str(-Inf));
fprintf('int2str(NaN)  = [%s] (expect NaN)\n',  int2str(NaN));

fprintf('\n=== integer-class input + -0 ===\n');
fprintf('int2str(int8(5)) = [%s] (expect 5)\n', int2str(int8(5)));
fprintf('int2str(-0)      = [%s] (expect 0)\n', int2str(-0));
