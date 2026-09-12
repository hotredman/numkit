clear;

% 3-D surf / mesh / waterfall export (portion 40; MATLAB R2025b
% probes: surf FaceColor 'flat' (quad colour = FIRST corner's Z),
% edges (0.129)x3; mesh EdgeColor 'flat' (segments by Z); clim auto =
% [min Z, max Z]; default view (-37.5, 30); painter's sort).

figure; surf(peaks(20));
saveas(gcf, [tempdir 'smoke_surf.png']);
n = numel(dir([tempdir 'smoke_surf.png']));
fprintf('surf png = %d (expect 1)\n', n);

figure; mesh(peaks(20));
saveas(gcf, [tempdir 'smoke_mesh.png']);
n = numel(dir([tempdir 'smoke_mesh.png']));
fprintf('mesh png = %d (expect 1)\n', n);

figure; waterfall(peaks(15));
saveas(gcf, [tempdir 'smoke_wf.png']);
n = numel(dir([tempdir 'smoke_wf.png']));
fprintf('waterfall png = %d (expect 1)\n', n);

% Explicit view round-trip (view az el is honoured by the renderer).
figure; surf(peaks(20)); view(0, 90);
saveas(gcf, [tempdir 'smoke_surftop.png']);
n = numel(dir([tempdir 'smoke_surftop.png']));
fprintf('top-view png = %d (expect 1)\n', n);
