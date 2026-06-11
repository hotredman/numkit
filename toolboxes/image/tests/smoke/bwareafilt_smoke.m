clear

import compat.*

% bwareafilt — keep components by area.

a2d = [1   0   0   0   0   0   1   0   0   1
       1   0   0   1   0   1   0   1   0   1
       1   0   1   0   0   0   0   0   0   0
       0   0   0   0   0   0   0   0   0   0
       0   1   0   0   0   0   0   0   0   0
       1   1   0   1   1   1   0   0   0   0
       1   1   0   1   0   0   0   1   0   0
       1   1   0   0   0   0   1   0   1   0
       1   1   0   0   0   0   0   0   0   0
       1   1   0   0   0   1   1   0   0   1];

% --- top-2 largest (Octave-source reference) ---
fprintf('--- bwareafilt(a2d, 2) — top-2 largest ---\n');
J = bwareafilt(a2d, 2);
disp(double(J));
fprintf('  expect: only the two largest blobs (left column + small cluster)\n\n');

% --- top-2 with explicit conn=4 ---
fprintf('--- bwareafilt(a2d, 2, 4) — top-2 with 4-conn ---\n');
disp(double(bwareafilt(a2d, 2, 4)));

% --- range [3, 5] ---
fprintf('\n--- bwareafilt(a2d, [3 5]) — components with 3-5 pixels ---\n');
disp(double(bwareafilt(a2d, [3 5])));

% --- "smallest" 1 ---
fprintf('\n--- bwareafilt(a2d, 1, "smallest") ---\n');
disp(double(bwareafilt(a2d, 1, 'smallest')));
