clear
import compat.*

% mat2str on complex values — re±|im|i per element (vs MATLAB R2025b).
fprintf('%s\n', mat2str(1+2i));               % 1+2i
fprintf('%s\n', mat2str([1+2i 3-4i]));        % [1+2i 3-4i]
fprintf('%s\n', mat2str([1+2i; 3-4i]));       % [1+2i;3-4i]
fprintf('%s\n', mat2str([1.5+2.25i -3i], 5)); % [1.5+2.25i 0-3i]
fprintf('%s\n', mat2str(complex(1, 0)));      % 1  (all imag zero → real)
fprintf('%s\n', mat2str([1 2 3]));            % [1 2 3]  (real path intact)

% DEEP-PROBE 2026-05-31: each element formatted INDEPENDENTLY — a zero-imag
% element inside an otherwise-complex array prints as a bare real (no "+0i").
fprintf('%s\n', mat2str([complex(1,1) complex(5,0)]));   % [1+1i 5]   not [1+1i 5+0i]
fprintf('%s\n', mat2str(complex([5 3],[0 4])));          % [5 3+4i]   not [5+0i 3+4i]
fprintf('%s\n', mat2str(sort([3+4i 1+1i 5 2-2i]), 6));   % [1+1i 2-2i 5 3+4i]
fprintf('%s\n', mat2str(complex([1 2],[0 0])));          % [1 2]   (all imag zero → real)
