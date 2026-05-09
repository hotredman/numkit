clear
import compat.*

fprintf('=== magic(N) demo: rows/cols/diagonals all sum to N(N^2+1)/2 ===\n\n');

for n = [3 4 5 6 8]
    M = magic(n);
    target = n * (n^2 + 1) / 2;
    rsum_ok = all(sum(M, 2) == target);
    csum_ok = all(sum(M, 1) == target);
    d1_ok = (sum(diag(M))           == target);
    d2_ok = (sum(diag(fliplr(M)))   == target);
    perm_ok = isequal(sort(M(:))', 1:n^2);
    fprintf('  N=%2d  target=%4d  rows=%d cols=%d diag1=%d diag2=%d perm=%d\n', ...
            n, target, rsum_ok, csum_ok, d1_ok, d2_ok, perm_ok);
end

fprintf('\nmagic(4) =\n');
disp(magic(4));

fprintf('Edge cases:\n');
fprintf('  magic(0) numel = %d (expect 0)\n', numel(magic(0)));
fprintf('  magic(1) value = %g (expect 1)\n', magic(1));
M2 = magic(2);
fprintf('  magic(2) = [%g %g; %g %g] (expect [1 3; 4 2] -- MATLAB convention)\n', ...
        M2(1,1), M2(1,2), M2(2,1), M2(2,2));
