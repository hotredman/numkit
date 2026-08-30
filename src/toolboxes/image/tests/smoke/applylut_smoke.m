clear

% applylut — apply n×n LUT to a binary image.

% --- 2x2 LUT (length 16) — Pratt bit-quad style ---
fprintf('--- applylut(eye(5), bweuler-style 2x2 LUT) ---\n');
% bweuler n=8 LUT for reference:
lut16 = [0; 1; 1; 0; 1; 0; -2; -1; 1; -2; 0; -1; 0; -1; -1; 0];
disp(applylut(eye(5), lut16));

% --- 3x3 LUT (length 512) — sum-of-neighbors >= 3 ---
fprintf('\n--- 3x3 LUT: sum >= 3 ---\n');
% Build a length-512 LUT directly: bit-i is on iff i has >= 3 bits set.
lut512 = zeros(512, 1);
for k = 0:511
    bits = 0;
    v = k;
    while v > 0
        bits = bits + bitand(v, 1);
        v = bitshift(v, -1);
    end
    if bits >= 3
        lut512(k + 1) = 1;
    end
end
S = applylut(eye(5), lut512);
disp(double(S));
fprintf('  expect: zeros (diagonal never has >= 3 neighbors with full 3x3 window)\n');

% --- 2x2 logical-LUT (output type follows LUT) ---
fprintf('\n--- 2x2 logical LUT ---\n');
lut_logical = logical([0; 1; 1; 0; 1; 0; 1; 1; 1; 1; 0; 1; 0; 1; 1; 0]);
result = applylut([1 0; 0 1], lut_logical);
fprintf('class = %s, size = %s\n', class(result), mat2str(size(result)));
disp(double(result));
