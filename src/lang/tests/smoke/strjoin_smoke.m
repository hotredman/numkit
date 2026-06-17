clear

import compat.*

% strjoin — cell-array delimiter (N-1 delimiters interleaved between elements).
% Cell-delimiter support added 2026-05-30 (DEEP-PROBE). vs MATLAB R2025b.

fprintf('=== cell array of delimiters ===\n');
fprintf('strjoin({a,b,c}, {", ", " and "}) = "%s" (expect a, b and c)\n', ...
        strjoin({'a','b','c'}, {', ', ' and '}));
fprintf('strjoin({x,y}, {"->"})            = "%s" (expect x->y)\n', ...
        strjoin({'x','y'}, {'->'}));
fprintf('strjoin({solo}, {})               = "%s" (expect solo)\n', ...
        strjoin({'solo'}, {}));

fprintf('\n=== single string delimiter (regress) ===\n');
fprintf('strjoin({a,b,c}, "-")  = "%s" (expect a-b-c)\n', strjoin({'a','b','c'}, '-'));
fprintf('strjoin({a,b,c})       = "%s" (expect a b c)\n',  strjoin({'a','b','c'}));
fprintf('strjoin({a,b,c}, ", ") = "%s" (expect a, b, c)\n', strjoin({'a','b','c'}, ', '));
