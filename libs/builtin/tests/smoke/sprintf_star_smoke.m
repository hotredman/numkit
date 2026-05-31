clear
import compat.*
% sprintf/fprintf C-style '*' field width / precision taken from the
% argument list (MATLAB supports this). DEEP-PROBE 2026-05-31: numkit used
% to print a garbage pointer for '%*d' because it never consumed the width
% arg. Values pinned vs MATLAB R2025b.

fprintf('[%*d]\n', 5, 42);             % expect [   42]  (width 5)
fprintf('[%.*f]\n', 3, 3.14159);       % expect [3.142]  (precision 3)
fprintf('[%*.*f]\n', 8, 2, 3.14159);   % expect [    3.14]  (width 8, prec 2)
fprintf('[%-*d]\n', 5, 42);            % expect [42   ]  (left-justified)

% The format recycles, consuming width+value per cycle.
fprintf('%s\n', sprintf('[%*d]', 4, 1, 4, 22, 4, 333)); % expect [   1][  22][ 333]

s = sprintf('%*.*f', 10, 3, pi);
fprintf('numel=%d (expect 10), s=[%s]\n', numel(s), s); % expect [     3.142]
