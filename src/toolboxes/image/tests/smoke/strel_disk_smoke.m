clear

% strel('disk',R,N) now matches MATLAB R2025b. MATLAB does NOT build a true
% Euclidean disk by default: for R>=3 it uses the radial periodic-line
% decomposition (Adams 1993 / Jones & Soille 1996) with N basis directions
% (default N=4), then pads with horizontal/vertical line strels. For R<3 or
% N=0 it falls back to the true Euclidean disk (xx^2+yy^2 <= R^2).
%
% Previously numkit always returned the full Euclidean disk, so e.g.
% strel('disk',5) gave an 11x11 mask (sum 81) instead of MATLAB's 9x9 (69).

se5 = strel('disk',5);  nh5 = se5.Neighborhood;
fprintf('--- strel(''disk'',5) default N=4 ---\n');
fprintf('size=%dx%d sum=%d   (expect 9x9, 69)\n', size(nh5,1), size(nh5,2), sum(nh5(:)));
fprintf('row sums: '); fprintf('%d ', sum(nh5,2)'); fprintf('  (expect 5 7 9 9 9 9 9 7 5)\n');

se3 = strel('disk',3);  nh3 = se3.Neighborhood;
fprintf('--- strel(''disk'',3) ---\n');
fprintf('size=%dx%d sum=%d all-ones=%d   (expect 5x5, 25, 1)\n', ...
        size(nh3,1), size(nh3,2), sum(nh3(:)), all(nh3(:)));

se7 = strel('disk',7);  nh7 = se7.Neighborhood;
fprintf('--- strel(''disk'',7) ---\n');
fprintf('size=%dx%d sum=%d   (expect 13x13, 157)\n', size(nh7,1), size(nh7,2), sum(nh7(:)));

% N=0 forces the true Euclidean disk (larger than the N=4 approximation).
se0 = strel('disk',5,0);  nh0 = se0.Neighborhood;
fprintf('--- strel(''disk'',5,0) Euclidean ---\n');
fprintf('size=%dx%d sum=%d   (expect 11x11, 81)\n', size(nh0,1), size(nh0,2), sum(nh0(:)));

% r<3 always Euclidean.
se2 = strel('disk',2);  nh2 = se2.Neighborhood;
fprintf('--- strel(''disk'',2) ---\n');
fprintf('size=%dx%d sum=%d   (expect 5x5, 13)\n', size(nh2,1), size(nh2,2), sum(nh2(:)));
