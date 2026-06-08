clear
import compat.*

fprintf('=== reducepoly (Ramer-Douglas-Peucker) ===\n');

P = [0 0; 1 0.05; 2 0.0; 3 1; 4 2; 5 2.05; 6 2];
R = reducepoly(P);
fprintf('default tol=0.001: %d -> %d points (expect 7 -> 6)\n', size(P,1), size(R,1));
fprintf('  kept: '); for i=1:size(R,1), fprintf('[%g %g] ', R(i,1), R(i,2)); end; fprintf('\n');
fprintf('  (the vertex [3 1], collinear with [2 0]-[4 2], is dropped)\n');

fprintf('tol=0.1:  %d points (expect 2 — endpoints only)\n', size(reducepoly(P,0.1),1));
fprintf('tol=1:    %d points (expect 2)\n', size(reducepoly(P,1),1));
fprintf('tol=0:    %d points (expect 6 — eps, minimal reduction)\n', size(reducepoly(P,0),1));

C = [0 0; 1 1; 2 2; 3 3; 4 4];
fprintf('collinear ramp: %d points (expect 2)\n', size(reducepoly(C,0.01),1));

T = [0 0; 1 1; 2 0; 3 1; 4 0];
fprintf('triangle wave:  %d points (expect 5 — all peaks kept)\n', size(reducepoly(T,0.1),1));

Pi = int32([0 0; 1 0; 2 5; 3 0]);
Ri = reducepoly(Pi, 0.1);
fprintf('int32 input: class=%s out=%d\n', class(Ri), size(Ri,1));

fprintf('\n=== validation ===\n');
try; reducepoly(P, 2); catch e; fprintf('tol=2: %s\n', strtok(e.message, char(10))); end
