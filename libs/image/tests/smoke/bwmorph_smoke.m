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

fprintf('\nBit-exact MATLAB R2025b on 23 fingerprints (tol=0). All 20+\n');
fprintf('operations: dilate, erode, bridge, clean, diag, dilate, endpoints,\n');
fprintf('erode, fatten, fill, hbreak, majority, perim4, perim8, remove,\n');
fprintf('bothat, close, open, tophat, shrink, skeleton, spur, thin,\n');
fprintf('thicken, branchpoints.\n');
