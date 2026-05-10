% geoplot — Geographic Plots (no basemap)
%
% v1 routes through plot/scatter with (X = lon, Y = lat) and
% auto-sets xlabel='lon' / ylabel='lat'. Basemap-tile rendering
% remains a follow-up.

import compat.*;

clear
close all

% --- City coordinates (lat, lon) ---
%   San Francisco — 37.7° N, 122.4° W
%   New York      — 40.7° N,  74.0° W
%   London        — 51.5° N,   0.1° W
%   Paris         — 48.8° N,   2.3° E
%   Tokyo         — 35.7° N, 139.7° E
lat = [37.7  40.7  51.5  48.8  35.7];
lon = [-122.4 -74.0 -0.1  2.3  139.7];

figure;
geoscatter(lat, lon, [120 100 80 80 100]);
title('5 capitals — geoscatter with size = relative population');

figure;
geoplot(lat, lon);
title('Same cities — connected as a line trace');

% --- Densely-sampled curve ---
t = linspace(0, 2*pi, 100);
figure;
geoplot(30 + 10 * sin(t), 100 * t / (2*pi) - 50);
title('Synthetic sinusoidal track over the equator');

fprintf('geoplot demo: 3 figures over (lat, lon) coordinates\n');
