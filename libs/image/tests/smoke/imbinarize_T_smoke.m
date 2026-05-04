clear

import compat.*

% --- Scalar threshold (existing path, regression check) ---
I = uint8([10 50 90 130 170; 200 230 240 250 255]);
BW = imbinarize(I, 0.5);   % normalised threshold
fprintf('--- imbinarize(I, 0.5) — scalar threshold (regression) ---\n');
fprintf('  size = %dx%d\n', size(BW, 1), size(BW, 2));
fprintf('  count(BW) = %d (expect: pixels with normalized value > 0.5)\n', sum(BW(:)));
% Check: pixels with normalised value > 0.5 ⇒ true.
expected = sum(double(I(:)) > 127.5);
fprintf('  matches scalar criterion? %d (expect 1)\n\n', sum(BW(:)) == expected);

% --- Per-pixel threshold = constant 0.5 (matrix form, equivalent to scalar) ---
T_const = 0.5 * ones(size(I));
BW_T = imbinarize(I, T_const);
fprintf('--- imbinarize(I, 0.5*ones(...)) — matrix threshold ---\n');
fprintf('  matches scalar version? %d (expect 1)\n\n', ...
    sum(BW(:) ~= BW_T(:)) == 0);

% --- Per-pixel threshold from adaptthresh: full pipeline ---
img = uint8([
    20 25 30 30 25 20;
    25 30 35 35 30 25;
    30 35 200 200 35 30;
    30 35 200 200 35 30;
    25 30 35 35 30 25]);
% Image has a bright 2x2 region at center surrounded by dim background.
% adaptthresh should recommend a threshold just below the local mean,
% making the bright center clearly cross.
T = adaptthresh(img, 0.5, 3);   % 3x3 window
BW = imbinarize(img, T);
fprintf('--- adaptthresh + imbinarize pipeline ---\n');
fprintf('  size(BW) = %dx%d\n', size(BW, 1), size(BW, 2));
fprintf('  count(BW) = %d (bright center should fire as foreground)\n', sum(BW(:)));
fprintf('  BW(3, 3) = %d (expect 1, bright center pixel)\n', BW(3, 3));
fprintf('  BW(1, 1) = %d (expect 0, dim corner)\n', BW(1, 1));

% --- Mismatched sizes raise ---
try
    imbinarize(uint8(zeros(3, 3)), zeros(2, 2));
    fprintf('\n--- BUG: shape mismatch did not throw\n');
catch err
    fprintf('\n--- shape mismatch correctly throws: %s\n', err.message);
end
