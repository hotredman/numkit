clear
import compat.*

% histcounts automatic bin selection (bugs/math/histcounts-autobinning) — these
% forms used to throw "bin edges required"; now they follow MATLAB binpicker.

% Default 'auto', integer data of small range -> unit bins centered on integers
[n1, e1] = histcounts([1 2 2 3 3 3]);
fprintf('auto integer : n=[%s]  e=[%s]   (expect n=[1 2 3], e=[0.5 1.5 2.5 3.5])\n', num2str(n1), num2str(e1));

% Explicit bin count
[n2, e2] = histcounts([1 2 3 4 5 6 7 8 9 10], 3);
fprintf('nbins = 3    : n=[%s]  e=[%s]   (expect n=[3 4 3], e=[0 4 8 12])\n', num2str(n2), num2str(e2));

% Default 'auto' on continuous data -> Scott's normal-reference rule
[n3, e3] = histcounts((1:200)/7);
fprintf('auto Scott   : %d bins on [%g, %g]   (expect 6 bins on [0, 30])\n', numel(n3), e3(1), e3(end));

% Fixed 'BinWidth'
[n4, e4] = histcounts([1 5 2 8 3], 'BinWidth', 2);
fprintf('BinWidth = 2 : n=[%s]  e=[%s]   (expect n=[1 2 1 1], e=[0 2 4 6 8])\n', num2str(n4), num2str(e4));

% 'BinLimits' + 'NumBins'
[n5, e5] = histcounts(1:5, 'BinLimits', [0 6], 'NumBins', 3);
fprintf('BinLimits    : n=[%s]  e=[%s]   (expect n=[1 2 2], e=[0 2 4 6])\n', num2str(n5), num2str(e5));
