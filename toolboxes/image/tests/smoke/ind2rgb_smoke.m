clear

import compat.*

% ind2rgb — indexed image → RGB lookup.

img = [2 4 5; 3 2 5; 1 2 4];
map = [0.0 0.0 0.0;
       0.2 0.4 0.6;
       0.4 0.4 0.5;
       0.3 0.7 1.0;
       0.1 0.5 0.8];
rgb = ind2rgb(img, map);
fprintf('size = %s, expected MxNx3 = [3 3 3]\n', mat2str(size(rgb)));
fprintf('R(1,1) = %.4f (expect 0.2)\n', rgb(1, 1, 1));
fprintf('G(1,1) = %.4f (expect 0.4)\n', rgb(1, 1, 2));
fprintf('B(1,1) = %.4f (expect 0.6)\n', rgb(1, 1, 3));

% Octave-source vector check
fprintf('\n--- expected vs actual R plane ---\n');
expected_R = [0.2 0.3 0.1; 0.4 0.2 0.1; 0.0 0.2 0.3];
disp(rgb(:,:,1));
fprintf('match R = %d\n', max(abs(rgb(:,:,1)(:) - expected_R(:))) < 1e-10);

% Out-of-range integer index → clip to last
fprintf('\n--- uint8 + 0-based indexing ---\n');
rgb_u = ind2rgb(uint8(img - 1), map);
fprintf('match double form = %d\n', isequal(rgb, rgb_u));
