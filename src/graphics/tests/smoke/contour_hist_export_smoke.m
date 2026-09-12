clear;

% contour / contourf / histogram export (portion 39).

x = [1 1 2 2 2 3 5 5 9];
figure;
histogram(x);
saveas(gcf, [tempdir 'smoke_hist.png']);
n = numel(dir([tempdir 'smoke_hist.png']));
fprintf('histogram png = %d (expect 1)\n', n);

% Bin semantics (MATLAB probes): integer rule → 9 bins at x±0.5.
figure;
histogram(x, 'FaceColor', [1 0.5 0], 'EdgeColor', [0.2 0.2 0.2]);
saveas(gcf, [tempdir 'smoke_hist2.png']);
n = numel(dir([tempdir 'smoke_hist2.png']));
fprintf('pinned-color histogram png = %d (expect 1)\n', n);

figure;
contour(peaks(10));
saveas(gcf, [tempdir 'smoke_contour.png']);
n = numel(dir([tempdir 'smoke_contour.png']));
fprintf('contour png = %d (expect 1)\n', n);

figure;
contourf(peaks(10));
saveas(gcf, [tempdir 'smoke_contourf.png']);
n = numel(dir([tempdir 'smoke_contourf.png']));
fprintf('contourf png = %d (expect 1)\n', n);
