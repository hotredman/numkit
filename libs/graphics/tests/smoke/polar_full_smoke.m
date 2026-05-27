clear

import compat.*

% Polar plot parity smoke — exercises every polar function the
% engine now exposes after the May 2026 polar parity sweep. Each
% block calls a builtin; correctness is observed by inspecting the
% figure in the IDE (PolarPlot.jsx renderer). The smoke runs to
% completion if every call returns without throwing — figure
% rendering is the human's job afterwards.

% ── polarplot — line on polar axes ───────────────────────────────
figure(1);
theta = linspace(0, 2*pi, 200);
polarplot(theta, sin(2*theta));
title('polarplot — sin(2θ) rose curve');

% ── polarscatter — markers ───────────────────────────────────────
figure(2);
t = 2*pi*rand(1, 60);
r = rand(1, 60);
polarscatter(t, r);
title('polarscatter — 60 random points');

% ── polarbubblechart — size-encoded scatter ──────────────────────
figure(3);
t = linspace(0, 2*pi, 20);
r = 1 + 0.5*sin(3*t);
sz = 20 + 80*rand(1, 20);
polarbubblechart(t, r, sz);
title('polarbubblechart — variable-size markers');

% ── polarhistogram — radial bars ─────────────────────────────────
figure(4);
samples = 2*pi*rand(1, 500);
polarhistogram(samples, 36);
title('polarhistogram — 500 samples, 36 bins');

% ── rose — legacy alias / wedge histogram ────────────────────────
figure(5);
rose(2*pi*rand(1, 500), 20);
title('rose — 500 samples, 20 bins');

% ── compass — arrows from origin on polar ────────────────────────
figure(6);
U = [3 -1 2 0.5];
V = [1  2 -1 -0.5];
compass(U, V);
title('compass — 4 vector arrows from origin');

% ── Axis customization ──────────────────────────────────────────
figure(7);
polarplot(theta, 1 + 0.4*sin(5*theta));
rlim([0 2]);
thetalim([0 360]);
thetadir('clockwise');
thetazerolocation('top');
thetaticks([0 45 90 135 180 225 270 315]);
thetaticklabels({'N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'});
rticks([0 0.5 1 1.5 2]);
rticklabels({'0', 'half', '1', 'one-and-half', 'two'});
title('custom thetadir/thetazero/ticks/labels');

fprintf('=== Polar smoke OK — 7 figures created ===\n');
