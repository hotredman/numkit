clear
import compat.*

fprintf('=== integralBoxFilter3 (3-D box filter on summed-volume table) ===\n');

% Deterministic 5x5x5 volume, then its 3-D integral image.
V = reshape(1:125, 5, 5, 5);
J = integralImage3(V);
fprintf('integralImage3(5x5x5): size=[%d %d %d] (expect 6 6 6)\n', ...
        size(J,1), size(J,2), size(J,3));

% Default 3x3x3 box -> 3x3x3 mean.
B = integralBoxFilter3(J);
fprintf('default 3x3x3: size=[%d %d %d] B(1,1,1)=%g B(2,2,2)=%g B(3,3,3)=%g (expect 3 3 3, 32, 63, 94)\n', ...
        size(B,1), size(B,2), size(B,3), B(1,1,1), B(2,2,2), B(3,3,3));

% Raw sum via NormalizationFactor = 1.
Bn1 = integralBoxFilter3(J, 3, 'NormalizationFactor', 1);
fprintf('NormalizationFactor=1 (raw sum): B(1,1,1)=%g (expect 864)\n', Bn1(1,1,1));

% 0.5 multiplier (MATLAB semantics: box-sum * normFactor).
Bh = integralBoxFilter3(J, 3, 'NormalizationFactor', 0.5);
fprintf('NormalizationFactor=0.5: B(1,1,1)=%g (expect 432)\n', Bh(1,1,1));

% Anisotropic [1 3 5] box -> output [5 3 1].
Bv = integralBoxFilter3(J, [1 3 5]);
fprintf('filterSize [1 3 5]: size=[%d %d %d] Bv(1,1,1)=%g (expect 5 3 1, 56)\n', ...
        size(Bv,1), size(Bv,2), size(Bv,3), Bv(1,1,1));

% Cross-check against direct imboxfilt3 mean in the valid (interior) region.
M = imboxfilt3(V, 3, 3, 3);   % same-size, replicate boundary
fprintf('cross-check vs imboxfilt3 interior: B(2,2,2)=%g M(3,3,3)=%g (expect equal, 63)\n', ...
        B(2,2,2), M(3,3,3));

fprintf('\n=== validation ===\n');
try; integralBoxFilter3(J, 2); catch e; fprintf('even size: %s\n', strtok(e.message, char(10))); end
try; integralBoxFilter3(J, 7); catch e; fprintf('too large: %s\n', strtok(e.message, char(10))); end
