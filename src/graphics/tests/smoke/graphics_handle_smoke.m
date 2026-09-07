clear;

% graphics_handle — plot-family handles, set/get, dot-syntax, gca/gcf,
% unified delete (portion 29).

h = plot(1:5, (1:5).^2);
fprintf('class(h) = %s (expect matlab.graphics.chart.primitive.Line)\n', class(h));
fprintf('get(h,''LineWidth'') = %g (expect 0.5)\n', get(h, 'LineWidth'));

set(h, 'LineWidth', 2, 'LineStyle', '--', 'Marker', 'o');
fprintf('after set: LineWidth=%g (expect 2), LineStyle=%s (expect --), Marker=%s (expect o)\n', ...
        get(h, 'LineWidth'), get(h, 'LineStyle'), get(h, 'Marker'));

h.LineWidth = 3;
fprintf('dot write: h.LineWidth = %g (expect 3)\n', h.LineWidth);
fprintf('h.Type = %s (expect line), h.Color = [%g %g %g] (expect [0 0.447 0.741])\n', ...
        h.Type, h.Color(1), h.Color(2), h.Color(3));

set(h, 'YData', [10 20 30 40 50]);
fprintf('YData(3) after set = %g (expect 30)\n', get(h, 'YData')(3));

a = gca;
fprintf('class(a) = %s (expect matlab.graphics.axis.Axes), a.Type = %s (expect axes)\n', ...
        class(a), a.Type);
set(a, 'XLim', [0 10], 'Title', 'Handle smoke');
fprintf('get(a,''XLim'') = [%g %g] (expect [0 10]), Title = %s\n', ...
        get(a, 'XLim'), get(a, 'Title'));

f = gcf;
fprintf('class(f) = %s (expect matlab.ui.Figure), f.Number = %g (expect 1)\n', ...
        class(f), f.Number);

hold on;
h2 = plot(1:5, 1:5);
fprintf('h == h2 = %g (expect 0), h == h = %g (expect 1)\n', h == h2, h == h);

hs = plot(1:5, 1:5, 1:5, 2:6);
fprintf('numel(hs) = %g (expect 2) — multi-series form\n', numel(hs));

delete(h2);
fprintf('after delete: ishandle(h2) = %g (expect 0), ishandle(h) = %g (expect 1)\n', ...
        ishandle(h2), ishandle(h));

s = get(h);
fprintf('get(h) struct: LineWidth = %g (expect 3), Type = %s (expect line)\n', ...
        s.LineWidth, s.Type);

fprintf('isa(h, ''handle'') = %g (expect 1), isgraphics(h) = %g (expect 1)\n', ...
        isa(h, 'handle'), isgraphics(h));
