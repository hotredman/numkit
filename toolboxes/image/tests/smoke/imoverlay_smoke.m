clear

import compat.*

% --- imoverlay on grayscale: paint a yellow square ---
I = uint8(128 * ones(5, 5));
BW = false(5, 5);
BW(2:4, 2:4) = true;
J = imoverlay(I, BW, [255 255 0]);   % byte color
fprintf('--- imoverlay grayscale + byte color ---\n');
fprintf('  size(J) = %dx%dx%d (expect 5x5x3)\n', size(J,1), size(J,2), size(J,3));
fprintf('  J(3,3,:) = [%d %d %d] (expect [255 255 0] yellow inside mask)\n', ...
    J(3,3,1), J(3,3,2), J(3,3,3));
fprintf('  J(1,1,:) = [%d %d %d] (expect [128 128 128] gray outside mask)\n\n', ...
    J(1,1,1), J(1,1,2), J(1,1,3));

% --- Float color [0..1] auto-detected ---
J2 = imoverlay(I, BW, [1.0 0.0 0.0]);
fprintf('--- float color [1 0 0] (auto-rescaled) ---\n');
fprintf('  J2(3,3,:) = [%d %d %d] (expect [255 0 0] red)\n\n', ...
    J2(3,3,1), J2(3,3,2), J2(3,3,3));

% --- RGB input + mask ---
RGB = uint8(zeros(3, 3, 3));
RGB(:,:,2) = 200;   % green-tinted
maskOne = false(3, 3);
maskOne(2, 2) = true;
J3 = imoverlay(RGB, maskOne, [0 0 255]);
fprintf('--- RGB input + mask one-pixel ---\n');
fprintf('  J3(2,2,:) = [%d %d %d] (expect [0 0 255] blue at masked pixel)\n', ...
    J3(2,2,1), J3(2,2,2), J3(2,2,3));
fprintf('  J3(1,1,:) = [%d %d %d] (expect [0 200 0] preserves original RGB)\n', ...
    J3(1,1,1), J3(1,1,2), J3(1,1,3));
