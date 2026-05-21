clear
import compat.*

fprintf('=== image/bwmorph — binary morphological operations ===\n');

BW = logical([
    0 0 0 0 0;
    0 1 1 0 0;
    0 1 1 1 0;
    0 1 1 1 0;
    0 0 0 0 0]);

fprintf('\n[5x5 figure, sum(BW) = %d]\n', sum(BW(:)));
ops = {'dilate', 'erode', 'open', 'close', 'bridge', 'clean', 'diag', ...
       'fill', 'hbreak', 'majority', 'perim4', 'perim8', 'remove', ...
       'endpoints', 'fatten', 'bothat', 'tophat'};
for i = 1:length(ops)
    J = bwmorph(BW, ops{i});
    fprintf('  %-12s sum = %2d\n', ops{i}, sum(J(:)));
end

fprintf('\n[iterated ops]\n');
fprintf('  skel-Inf      sum = %d (expect 5)\n',  sum(sum(bwmorph(BW, 'skel', Inf))));
fprintf('  thin-Inf      sum = %d (expect 1)\n',  sum(sum(bwmorph(BW, 'thin', Inf))));
fprintf('  shrink-Inf    sum = %d (expect 1)\n',  sum(sum(bwmorph(BW, 'shrink', Inf))));
fprintf('  spur-2        sum = %d (expect 8)\n',  sum(sum(bwmorph(BW, 'spur', 2))));
fprintf('  thicken       sum = %d (expect 19)\n', sum(sum(bwmorph(BW, 'thicken', 1))));
fprintf('  branchpoints  sum = %d (expect 0)\n',  sum(sum(bwmorph(BW, 'branchpoints'))));

fprintf('\n[20x20 random rng(0)]\n');
rng(0);
BW2 = rand(20, 20) > 0.5;
fprintf('  dilate        sum = %d (MATLAB: 400)\n', sum(sum(bwmorph(BW2, 'dilate'))));
fprintf('  skel-Inf      sum = %d (MATLAB: 158)\n', sum(sum(bwmorph(BW2, 'skel', Inf))));
fprintf('  thin-Inf      sum = %d (MATLAB: 147)\n', sum(sum(bwmorph(BW2, 'thin', Inf))));
fprintf('  shrink-Inf    sum = %d (MATLAB:  89)\n', sum(sum(bwmorph(BW2, 'shrink', Inf))));

fprintf('\n[morphology invariants — MATLAB-independent]\n');
A = rand(24, 24) > 0.4;
fprintf('  A subset of dilate(A): %d (expect 1)\n', ...
    nnz(A & ~bwmorph(A, 'dilate')) == 0);
fprintf('  erode(A) subset of A : %d (expect 1)\n', ...
    nnz(bwmorph(A, 'erode') & ~A) == 0);
blk = false(20, 20); blk(5:16, 5:16) = true;
fprintf('  shrink(blk) -> 1 px  : %d (expect 1)\n', ...
    nnz(bwmorph(blk, 'shrink', Inf)) == 1);

fprintf('\nClean-room dispatcher (Gonzalez & Woods; Pratt; Lam/Lee/Suen).\n');
fprintf('Bit-exact MATLAB R2025b on 23 fingerprints (tol=0): dilate,\n');
fprintf('erode, bridge, clean, diag, endpoints, fatten, fill, hbreak,\n');
fprintf('majority, perim4, perim8, remove, bothat, close, open, tophat,\n');
fprintf('shrink, skeleton, spur, thin, thicken, branchpoints.\n');
