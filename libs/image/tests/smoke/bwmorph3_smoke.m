clear
import compat.*

fprintf('=== bwmorph3 (3-D binary morphology) ===\n');

% 3x3x3 solid cube with the centre voxel removed (26 voxels set).
V = false(5,5,5); V(2:4,2:4,2:4) = true; V(3,3,3) = false;

fprintf('cube-minus-hole (26 set):\n');
fprintf('  branchpoints nnz=%d (expect 26 — dense blob)\n', nnz(bwmorph3(V,'branchpoints')));
fprintf('  clean        nnz=%d (expect 26 — none isolated)\n', nnz(bwmorph3(V,'clean')));
fprintf('  endpoints    nnz=%d (expect 0)\n', nnz(bwmorph3(V,'endpoints')));
fprintf('  fill         nnz=%d (expect 27 — hole filled)\n', nnz(bwmorph3(V,'fill')));
fprintf('  majority     nnz=%d (expect 7)\n', nnz(bwmorph3(V,'majority')));
fprintf('  remove       nnz=%d (expect 26)\n', nnz(bwmorph3(V,'remove')));

% A straight line of 5 voxels: endpoints = the two ends.
L = false(5,5,7); L(3,3,2:6) = true;
fprintf('z-line of 5: endpoints nnz=%d (expect 2)  branchpoints nnz=%d (expect 0)\n', ...
        nnz(bwmorph3(L,'endpoints')), nnz(bwmorph3(L,'branchpoints')));

% 2-D input is treated as a single-plane volume.
B = logical([0 1 1 0; 1 1 1 1; 0 1 1 0; 0 0 1 0]);
J = bwmorph3(B, 'clean');
fprintf('2-D clean: class=%s size=[%d %d] nnz=%d\n', class(J), size(J,1), size(J,2), nnz(J));

fprintf('\n=== validation ===\n');
try; bwmorph3(V, 'bogus'); catch e; fprintf('bad op: %s\n', strtok(e.message, char(10))); end
