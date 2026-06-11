clear

import compat.*

% circshift is a pure rearrangement, so it should work on ANY element type.
% DEEP-PROBE 2026-05-31: numkit threw "Not a double array" on char / cell /
% logical / complex / single inputs. Now type-agnostic. Reference: MATLAB
% R2025b. (STRING + struct arrays are deferred.)

fprintf('=== char vector ===\n');
fprintf('circshift(abcde,2)  = [%s]  (expect deabc)\n', circshift('abcde', 2));
fprintf('circshift(abcde,-1) = [%s]  (expect bcdea)\n', circshift('abcde', -1));

fprintf('\n=== cell + cellstr ===\n');
c = circshift({1,2,3,4}, 1);
fprintf('circshift({1,2,3,4},1) = {%g %g %g %g}  (expect 4 1 2 3)\n', c{1}, c{2}, c{3}, c{4});
cs = circshift({'x','y','z'}, 1);
fprintf('circshift({x,y,z},1)   = {%s %s %s}  (expect z x y)\n', cs{1}, cs{2}, cs{3});

fprintf('\n=== char matrix (shifts rows) + [r c] ===\n');
M = circshift(['ab';'cd';'ef'], 1);
fprintf('rows shifted: row1 = [%s]  (expect ef)\n', M(1,:));
M2 = circshift(['abc';'def'], [1 1]);
fprintf('[1 1]: row1 = [%s]  (expect fde)\n', M2(1,:));

fprintf('\n=== logical / complex / single ===\n');
fprintf('logical: %s  (expect [1 0 1 0])\n', mat2str(double(circshift([true false true false], 2))));
zc = circshift([1+1i 2+2i 3+3i], 1);
fprintf('complex re: %s  (expect [3 1 2])\n', mat2str(real(zc)));
fprintf('single: %s (class %s)  (expect [3 1 2], single)\n', mat2str(double(circshift(single([1 2 3]), 1))), class(circshift(single([1 2 3]), 1)));

fprintf('\n=== double path unchanged ===\n');
disp(circshift([1 2 3 4 5], 2));   % expect [4 5 1 2 3]
