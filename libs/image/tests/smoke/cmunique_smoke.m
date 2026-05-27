clear;
import compat.*;

fprintf('--- (1) (X, MAP) form — magic(4) with duplicated gray(8) cmap ---\n');
X = magic(4);
map = [gray(8); gray(8)];   % 16 rows, second half duplicates first
[Y, newmap] = cmunique(X, map);
fprintf('  Y (uint8, 0-based):\n');
for r = 1:4
    fprintf('  '); for c = 1:4; fprintf('%3u', Y(r,c)); end; fprintf('\n');
end
fprintf('  newmap size = [%d %d]  (expect 8x3 — duplicates removed)\n', ...
        size(newmap,1), size(newmap,2));

fprintf('\n--- (2) (RGB) form: 2x2 truecolor → indexed ---\n');
RGB = cat(3, [0.1 0.2; 0.1 0.3], [0.5 0.6; 0.5 0.7], [0.9 0.8; 0.9 0.6]);
[Y, newmap] = cmunique(RGB);
fprintf('  Y:\n');
for r = 1:2; fprintf('  '); for c = 1:2; fprintf('%3u', Y(r,c)); end; fprintf('\n'); end
fprintf('  newmap size = [%d %d]  (expect 3x3 — 3 unique colours)\n', ...
        size(newmap,1), size(newmap,2));

fprintf('\n--- (3) (I) form: 2x2 intensity → indexed ---\n');
I = [0.1 0.2; 0.1 0.3];
[Y, newmap] = cmunique(I);
fprintf('  Y:\n');
for r = 1:2; fprintf('  '); for c = 1:2; fprintf('%3u', Y(r,c)); end; fprintf('\n'); end
fprintf('  newmap size = [%d %d]  (expect 3x3 — 3 distinct intensities)\n', ...
        size(newmap,1), size(newmap,2));

fprintf('\n--- (4) uint8 X — already integer; cmap has duplicate row 4 ---\n');
X = uint8([1 2; 3 2]);
map = [0 0 0; 0.5 0.5 0.5; 1 1 1; 0.5 0.5 0.5];
[Y, newmap] = cmunique(X, map);
fprintf('  Y:\n');
for r = 1:2; fprintf('  '); for c = 1:2; fprintf('%3u', Y(r,c)); end; fprintf('\n'); end
fprintf('  newmap (expect 2x3 — only used rows kept):\n');
disp(newmap);
