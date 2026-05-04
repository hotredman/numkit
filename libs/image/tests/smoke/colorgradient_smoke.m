clear

import compat.*

% colorgradient — smooth interpolated colormap.

% --- doc example: blue -> yellow -> red, n=64 default ---
fprintf('--- colorgradient([0,0,1; 1,1,0; 1,0,0]) ---\n');
M = colorgradient([0 0 1; 1 1 0; 1 0 0]);
fprintf('size: %s\n', mat2str(size(M)));
fprintf('first row (anchor blue): %s\n', mat2str(M(1, :)));
fprintf('middle (anchor yellow): %s\n', mat2str(M(round(end/2), :)));
fprintf('last row (anchor red):  %s\n', mat2str(M(end, :)));

fprintf('\n--- explicit n=8 ---\n');
M8 = colorgradient([0 0 1; 1 0 0], 8);
disp(M8);

fprintf('--- weighted: [2;1] gives 2/3 to first interval ---\n');
Mw = colorgradient([0 0 1; 1 1 0; 1 0 0], [2;1], 10);
fprintf('size: %s\n', mat2str(size(Mw)));
disp(Mw);
