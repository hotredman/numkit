clear
import compat.*

% fsamp2 / ftrans2 / fwind1 / fwind2 — 2-D FIR filter design.

[f1, f2] = freqspace(5, 'meshgrid');
Hd = ones(5);
Hd(abs(f1) > 0.5 | abs(f2) > 0.5) = 0;

fprintf('=== fsamp2(Hd) ===\n');
h = fsamp2(Hd);
fprintf('class=%s size=[%d %d] sum=%.6f (expect 1.0)\n', class(h), size(h,1), size(h,2), sum(h(:)));

fprintf('\n=== ftrans2(b) McClellan ===\n');
b = [-1 -2 -1 8 -1 -2 -1]/4;
h = ftrans2(b);
fprintf('size=[%d %d] h(1,1)=%.10f (expect -0.00390625)\n', size(h,1), size(h,2), h(1,1));

fprintf('\n=== ftrans2(b, t) custom ===\n');
t = [0 1 0; 1 0 1; 0 1 0]/4;
h = ftrans2(b, t);
fprintf('h(4,4)=%.6f (expect 2.5)\n', h(4,4));

fprintf('\n=== fwind1 Huang ===\n');
n = 5; w1 = 0.5 - 0.5*cos(2*pi*(0:n-1)/(n-1));
h = fwind1(Hd, w1(:));
fprintf('size=[%d %d] sum=%.6f\n', size(h,1), size(h,2), sum(h(:)));

fprintf('\n=== fwind1 separable ===\n');
h = fwind1(Hd, w1(:), w1(:));
fprintf('size=[%d %d]\n', size(h,1), size(h,2));

fprintf('\n=== fwind2 ===\n');
W = w1(:) * w1(:).';
h = fwind2(Hd, W);
fprintf('size=[%d %d]\n', size(h,1), size(h,2));
