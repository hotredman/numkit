clear;

% findobj / quiver / annotation (portion 41).

figure;
h1 = plot(1:10);
hold on;
h2 = plot(1:10, 'r');
t = title('T');
fprintf('root lines = %d (expect 2)\n', numel(findobj('Type','line')));
fprintf('axes lines = %d (expect 2)\n', numel(findobj(gca,'Type','line')));
fprintf('texts = %d (expect 0 — MATLAB hides titles)\n', numel(findobj('Type','text')));
fprintf('self scope = %d (expect 1)\n', numel(findobj(h2)));
fprintf('red lines = %d (expect 1)\n', numel(findobj('Color','r')));
c = get(h2, 'Color');
fprintf('color = [%g %g %g] (expect [1 0 0])\n', c(1), c(2), c(3));

close all;
figure;
quiver([1 2 3], [1 1 1], [0.5 0 -0.5], [0 0.5 0]);
annotation('arrow', [0.2 0.8], [0.2 0.8]);
annotation('textarrow', [0.15 0.7], [0.8 0.6], 'String', 'note', 'Color', 'r');
annotation('ellipse', [0.55 0.1 0.15 0.15], 'FaceColor', [1 1 0]);
saveas(gcf, [tempdir 'smoke_fqa.png']);
n = numel(dir([tempdir 'smoke_fqa.png']));
fprintf('png = %d (expect 1)\n', n);
