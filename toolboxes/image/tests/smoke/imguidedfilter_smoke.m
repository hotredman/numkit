clear
import compat.*

% imguidedfilter — edge-preserving Guided Image Filter (He et al. 2013).
% Bit-equal MATLAB R2025b at 1e-10 tolerance.

I = double(reshape(1:25, 5, 5)) / 25;

fprintf('=== default (self-guide, NHood=5, eps=0.01) ===\n');
B = imguidedfilter(I);
fprintf('B(3,3) = %.6f (expect 0.520000)\n', B(3,3));
fprintf('B(1,1) = %.8f (expect 0.08373697)\n', B(1,1));

fprintf('\n=== NeighborhoodSize = 3 ===\n');
B = imguidedfilter(I, 'NeighborhoodSize', 3);
fprintf('B(3,3) = %.6f (expect 0.520000)\n', B(3,3));

fprintf('\n=== custom DegreeOfSmoothing = 0.01 ===\n');
B = imguidedfilter(I, 'DegreeOfSmoothing', 0.01);
fprintf('B(3,3) = %.6f (expect 0.520000)\n', B(3,3));

fprintf('\n=== cross-guidance ===\n');
G = double([0 0 0 0 0; 0 1 1 1 0; 0 1 1 1 0; 0 1 1 1 0; 0 0 0 0 0]);
B = imguidedfilter(I, G);
fprintf('B(3,3) = %.6f (expect 0.520000)\n', B(3,3));

fprintf('\n=== uint8 ===\n');
Iu = uint8(I * 255);
B = imguidedfilter(Iu);
fprintf('B(3,3) = %d (expect 133) class=%s\n', B(3,3), class(B));
