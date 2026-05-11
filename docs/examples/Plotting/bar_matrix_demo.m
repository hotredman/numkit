% bar(matrix) — Grouped + Stacked Multi-Series Bars
%
% Pass a matrix Y to bar() and each column becomes its own series.
% Default mode is 'grouped' (sub-pixel offset per series). Add
% 'stacked' to pile series cumulatively.

import compat.*;

clear
close all

% Synthetic per-quarter sales for 3 regions across 4 years.
%   rows = years    (2022, 2023, 2024, 2025)
%   cols = regions  (Americas, EMEA, Asia)
Y = [180 120 90; ...
     200 150 130; ...
     220 180 160; ...
     250 210 220];
years = 2022:2025;

figure;
bar(years, Y);
title('Grouped — 3 regions × 4 years');
xlabel('year'); ylabel('sales (M$)');
legend('Americas', 'EMEA', 'Asia');

figure;
bar(years, Y, 'stacked');
title('Stacked — same data, cumulative bars');
xlabel('year'); ylabel('sales (M$)');
legend('Americas', 'EMEA', 'Asia');

% --- area(matrix) — cumulative-sum stacked area variant ---
figure;
area(years, Y);
title('area(Y) — stacked-area equivalent of stacked bar');
xlabel('year'); ylabel('sales (M$)');
legend('Americas', 'EMEA', 'Asia');

fprintf('bar(matrix) demo: 3 figures (grouped + stacked + area)\n');
