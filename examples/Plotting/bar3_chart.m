% bar3 — 3-D bar chart
% Each cell of the matrix becomes a coloured cuboid centred over
% its (column, row) position. Heights drive the color via the same
% height-mapped palette as surf, so taller bars show up red and
% shorter ones blue.
import compat.*;

clear
close all

% Quarterly sales for 4 products over 5 quarters.
sales = [12  18  22  20  25;
          8  10  16  18  20;
         15  14  18  22  28;
         10  12  14  17  20];

figure
bar3(sales)
title('Quarterly Sales by Product')
xlabel('Quarter')
ylabel('Product')
zlabel('Units sold')
grid on
view(-37.5, 30)
