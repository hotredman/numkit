clear
import compat.*

fprintf('=== griddatan — N-D scattered-data interpolation ===\n');

% 2-D linear (default method) — corners of unit square.
fprintf('\n2-D linear @ unit-square corners (default method):\n');
X = [1 0; 0 0; 1 1; 0 1];
v = [1; 0; 2; 1];
fprintf('   griddatan(X, v, [0.5 0.5]) = %g  (expect ~1)\n', ...
        griddatan(X, v, [0.5 0.5]));

% 2-D nearest — same square, off-center query.
fprintf('\n2-D nearest:\n');
fprintf('   griddatan(X, v, [0.4 0.4], ''nearest'') = %g  (closest = (0,0), v=0)\n', ...
        griddatan(X, v, [0.4 0.4], 'nearest'));

% 3-D nearest — tetrahedron of unit basis vectors + origin.
fprintf('\n3-D nearest:\n');
X3 = [0 0 0; 1 0 0; 0 1 0; 0 0 1];
v3 = [1; 2; 3; 4];
fprintf('   griddatan(X3, v3, [0.1 0.1 0.1], ''nearest'') = %g  (closest = origin)\n', ...
        griddatan(X3, v3, [0.1 0.1 0.1], 'nearest'));

% 3-D linear is a KNOWN GAP — should error with a clear message.
fprintf('\n3-D linear (expect error — N-D Delaunay is a v1 gap):\n');
try
    griddatan(X3, v3, [0.1 0.1 0.1], 'linear');
    fprintf('   FAIL: should have thrown\n');
catch ME
    fprintf('   OK: %s\n', ME.message);
end

% Multiple queries.
fprintf('\nMultiple queries (2-D linear):\n');
xq = [0.25 0.25; 0.5 0.5; 0.75 0.75];
disp(griddatan(X, v, xq));
