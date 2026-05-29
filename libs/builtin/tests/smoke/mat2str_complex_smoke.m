clear
import compat.*

% mat2str on complex values — re±|im|i per element (vs MATLAB R2025b).
fprintf('%s\n', mat2str(1+2i));               % 1+2i
fprintf('%s\n', mat2str([1+2i 3-4i]));        % [1+2i 3-4i]
fprintf('%s\n', mat2str([1+2i; 3-4i]));       % [1+2i;3-4i]
fprintf('%s\n', mat2str([1.5+2.25i -3i], 5)); % [1.5+2.25i 0-3i]
fprintf('%s\n', mat2str(complex(1, 0)));      % 1  (all imag zero → real)
fprintf('%s\n', mat2str([1 2 3]));            % [1 2 3]  (real path intact)
