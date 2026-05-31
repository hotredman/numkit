clear

import compat.*

% num2str of a SCALAR complex value (DEEP-PROBE 2026-05-31). Previously
% num2str(complex) THREW "Cannot convert complex with nonzero imaginary part
% to double scalar". MATLAB formats re±|im|i with a COMMON precision derived
% from max(|re|,|im|); a zero-imag element prints as a bare real. vs MATLAB
% R2025b. (Complex ARRAY num2str uses column alignment and is deferred.)

fprintf('=== default precision ===\n');
fprintf('num2str(3-1i)   = [%s]  (expect 3-1i)\n', num2str(3-1i));
fprintf('num2str(-2-3i)  = [%s]  (expect -2-3i)\n', num2str(-2-3i));
fprintf('num2str(0+1i)   = [%s]  (expect 0+1i)\n', num2str(0+1i));
fprintf('num2str(5+0i)*  = [%s]  (expect 5, zero imag -> bare real)\n', num2str(complex(5,0)));
fprintf('num2str(1.23456789+9.87654321i) = [%s]  (expect 1.2346+9.8765i)\n', num2str(1.23456789+9.87654321i));
fprintf('num2str(1234.5+6.789012i)       = [%s]  (expect 1234.5+6.789012i, common prec 8)\n', num2str(1234.5+6.789012i));

fprintf('\n=== N significant digits ===\n');
fprintf('num2str(pi+2.5i,8) = [%s]  (expect 3.1415927+2.5i)\n', num2str(pi+2.5i, 8));
fprintf('num2str(1+2i,3)    = [%s]  (expect 1+2i)\n', num2str(1+2i, 3));

fprintf('\n=== printf format (per part) ===\n');
fprintf('num2str(3.14159-2.71828i,''%%.3f'') = [%s]  (expect 3.142-2.718i)\n', num2str(3.14159-2.71828i, '%.3f'));
