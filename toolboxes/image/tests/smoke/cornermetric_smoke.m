clear
import compat.*

% cornermetric — Harris & Shi-Tomasi corner detector.
% Reference values from MATLAB R2025b at 1e-7 tolerance.

I = double([0 0 0 0 0; 0 1 1 0 0; 0 1 1 0 0; 0 0 0 0 0; 0 0 0 0 0]);

fprintf('=== Harris (default, k=0.04) ===\n');
C = cornermetric(I);
fprintf('C(1,1) = %.6f (expect 0.506653)\n', C(1,1));
fprintf('C(2,2) = %.6f (expect 0.621522)\n', C(2,2));
fprintf('C(3,3) = %.6f (expect 0.350575)\n', C(3,3));

fprintf('\n=== MinimumEigenvalue (Shi-Tomasi) ===\n');
C = cornermetric(I, 'MinimumEigenvalue');
fprintf('C(2,2) = %.6f (expect 0.710046)\n', C(2,2));
fprintf('C(3,3) = %.6f (expect 0.642212)\n', C(3,3));

fprintf('\n=== Harris with k = 0.1 ===\n');
C = cornermetric(I, 'Harris', 'SensitivityFactor', 0.1);
fprintf('C(2,2) = %.6f (expect 0.435699)\n', C(2,2));

fprintf('\n=== Custom FilterCoefficients = [1 1 1]/3 ===\n');
C = cornermetric(I, 'Harris', 'FilterCoefficients', [1 1 1]/3);
fprintf('C(2,2) = %.6f (expect 0.827654)\n', C(2,2));

fprintf('\n=== uint8 input class ===\n');
Iu = uint8(I * 255);
C = cornermetric(Iu);
fprintf('C(2,2) = %.6f (expect 0.621522) class=%s\n', C(2,2), class(C));

fprintf('\n=== peaks(8) ===\n');
Ip = peaks(8);
C = cornermetric(Ip);
fprintf('C(4,4) = %.4f (expect 80.7075)\n', C(4,4));
fprintf('C(5,5) = %.4f (expect 92.0254)\n', C(5,5));
