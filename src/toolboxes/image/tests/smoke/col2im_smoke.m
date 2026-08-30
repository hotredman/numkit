clear

% --- Round-trip distinct: col2im(im2col(A, 'distinct'), 'distinct') == A ---
% Multiples-case (A's dims are exact multiples of block).
A = reshape(1:16, [4 4]);
B = im2col(A, [2 2], 'distinct');
A_back = col2im(B, [2 2], [4 4], 'distinct');
fprintf('--- distinct round-trip on 4x4 (clean multiples) ---\n');
fprintf('  max|A_back - A| = %.6e (expect 0)\n\n', ...
    max(max(abs(A_back - A))));

% --- Round-trip distinct on 5x4 (zero-pad row): pad cells dropped ---
A2 = reshape(1:20, [5 4]);
B2 = im2col(A2, [2 2], 'distinct');
A2_back = col2im(B2, [2 2], [5 4], 'distinct');
fprintf('--- distinct round-trip on 5x4 (last row tiled+padded) ---\n');
fprintf('  size(A2_back) = [%d %d] (expect [5 4])\n', ...
    size(A2_back, 1), size(A2_back, 2));
fprintf('  max|A2_back - A2| = %.6e (expect 0 — pad cells dropped)\n\n', ...
    max(max(abs(A2_back - A2))));

% --- Sliding mode: B is a 1×N row of dot products ---
% Build a synthetic row that we want to reshape to (mm-m+1)×(nn-n+1).
mm = 4; nn = 5; m = 2; n = 2;
Hp = mm - m + 1; Wp = nn - n + 1;
B3 = reshape(1:(Hp*Wp), [1 Hp*Wp]);
A3 = col2im(B3, [m n], [mm nn], 'sliding');
fprintf('--- sliding-mode reshape ---\n');
fprintf('  size(A3) = [%d %d] (expect [%d %d])\n', ...
    size(A3, 1), size(A3, 2), Hp, Wp);
fprintf('  A3(1,1) = %d (expect 1)\n', A3(1, 1));
fprintf('  A3(end,end) = %d (expect %d)\n', A3(Hp, Wp), Hp * Wp);
fprintf('  max|A3 - reshape(B3, [%d %d])| = %.6e (expect 0)\n\n', ...
    Hp, Wp, max(max(abs(A3 - reshape(B3, [Hp Wp])))));

% --- Convolution-by-im2col-then-col2im idiom ---
% Same kernel as im2col_smoke; build im2col, dot with K, col2im(sliding).
K = [1 2; 3 4] / 10;
A4 = reshape(1:25, [5 5]);
expected = zeros(4, 4);
for r = 1:4
    for c = 1:4
        expected(r, c) = sum(sum(A4(r:r+1, c:c+1) .* K));
    end
end
M = im2col(A4, [2 2]);
row = K(:)' * M;
A4_back = col2im(row, [2 2], [5 5], 'sliding');
fprintf('--- im2col→K·B→col2im pipeline ---\n');
fprintf('  size(A4_back) = [%d %d] (expect [4 4])\n', ...
    size(A4_back, 1), size(A4_back, 2));
fprintf('  max|A4_back - explicit| = %.6e (expect ~0)\n\n', ...
    max(max(abs(A4_back - expected))));

% --- Bad-shape errors ---
ok1 = false;
try
    % Need 9 elements (3×3 sliding output), give 7 — must raise.
    col2im(reshape(1:7, [1 7]), [2 2], [4 4], 'sliding');
catch
    ok1 = true;
end
ok2 = false;
try
    col2im(reshape(1:8, [2 4]), [2 2], [4 4], 'distinct'); % wrong rows (need 4)
catch
    ok2 = true;
end
fprintf('--- bad-shape diagnostics ---\n');
fprintf('  wrong-numel sliding raises  = %d (expect 1)\n', ok1);
fprintf('  wrong-rows distinct raises  = %d (expect 1)\n', ok2);
