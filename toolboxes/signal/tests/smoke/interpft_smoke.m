clear

import compat.*

fprintf('=== interpft ===\n');

% Vector: 8-pt to 16-pt
y = interpft((1:8)', 16);
fprintf('  interpft(1:8, 16):\n    '); fprintf('%.4f ', y); fprintf('\n');
fprintf('  expect originals at odd indices (y(1)=1, y(3)=2, ..., y(15)=8)\n');

% Matrix dim=1 (default): each column interpolated
M = [1 5; 2 6; 3 7; 4 8];
y = interpft(M, 8);
fprintf('  interpft(M, 8) dim=1 default → %dx%d\n', size(y, 1), size(y, 2));
fprintf('    col 1 evens: %.4f %.4f %.4f %.4f (originals)\n', y(1,1), y(3,1), y(5,1), y(7,1));
fprintf('    col 2 evens: %.4f %.4f %.4f %.4f (originals)\n', y(1,2), y(3,2), y(5,2), y(7,2));

% Matrix dim=2: each row interpolated
y2 = interpft(M, 4, 2);
fprintf('  interpft(M, 4, 2) dim=2 → %dx%d\n', size(y2, 1), size(y2, 2));
fprintf('    row 1: '); fprintf('%.4f ', y2(1,:)); fprintf('\n');
fprintf('    row 2: '); fprintf('%.4f ', y2(2,:)); fprintf('\n');
