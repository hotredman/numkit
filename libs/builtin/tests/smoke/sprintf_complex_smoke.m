clear

import compat.*

% bugs/builtin/sprintf-complex.md — sprintf/fprintf use the REAL part of a
% complex argument for numeric conversions (imaginary discarded), like MATLAB.

fprintf('sprintf(''%%g'',1+2i) = [%s]   expect [1]\n', sprintf('%g', 1+2i));
fprintf('sprintf(''%%d '',[1+2i 3+4i]) = [%s]   expect [1 3 ]\n', sprintf('%d ', [1+2i 3+4i]));
fprintf('sprintf(''%%.2f'',3.5-1.5i) = [%s]   expect [3.50]\n', sprintf('%.2f', 3.5-1.5i));
fprintf('sprintf(''%%g '',[1.5+0i 2.5]) = [%s]   expect [1.5 2.5 ]\n', sprintf('%g ', [1.5+0i 2.5]));
fprintf('sprintf(''%%d'',complex(7,0)) = [%s]   expect [7]\n', sprintf('%d', complex(7,0)));

% Real arguments unaffected (regression).
fprintf('sprintf(''%%g'',1000000) = [%s]   expect [1e+06]\n', sprintf('%g', 1000000));
fprintf('sprintf(''%%.3f'',pi) = [%s]   expect [3.142]\n', sprintf('%.3f', pi));
