clear

% freqz 'whole': frequency grid spans [0, 2*pi) instead of [0, pi). vs MATLAB.
[h, w] = freqz([1 1], 1, 4, 'whole');
fprintf('whole w = [%g %g %g %g] (expect 0 1.5708 3.1416 4.7124)\n', w(1),w(2),w(3),w(4));
fprintf('whole |h(1)| = %g (expect 2)\n', abs(h(1)));
[~, wh] = freqz([1 1], 1, 4);
fprintf('half  w = [%g %g %g %g] (expect 0 0.7854 1.5708 2.3562)\n', wh(1),wh(2),wh(3),wh(4));
