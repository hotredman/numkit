clear
import compat.*

% Test 1: equal dispersion, interleaved (asymptotic, no ties).
a = [1 3 5 7 9 11 13 15 17 19]';
b = [2 4 6 8 10 12 14 16 18 20]';
[h, p, stats] = ansaribradley(a, b);
fprintf('det1: h=%d p=%.10f W=%g Wstar=%.6f  (MATLAB: 1.0, 55, 0.0)\n', ...
        h, p, stats.W, stats.Wstar);

% Test 2: b wider — exact path (m=5).
a = [-1 -0.5 0 0.5 1]';
b = [-5 -3 -1 1 3 5]';
[h, p, stats] = ansaribradley(a, b);
fprintf('det2: h=%d p=%.10f W=%g  (MATLAB: 1, 0.0259740260, 23)\n', ...
        h, p, stats.W);

% Test 3: ties — production-grade exact with LCM scaling.
a = [1 2 2 3 4]';
b = [2 3 3 4 5]';
[h, p, stats] = ansaribradley(a, b);
fprintf('det3 ties: h=%d p=%.10f W=%g  (MATLAB: 0, 0.8809523810, 14.167)\n', ...
        h, p, stats.W);

% Test 4: large asymptotic.
a = linspace(-2, 2, 20)';
b = linspace(-5, 5, 25)';
[h, p, stats] = ansaribradley(a, b);
fprintf('det4 large: h=%d p=%.10f W=%g  (MATLAB: 1, 0.0006293629, 310)\n', ...
        h, p, stats.W);

% Test 5: inverted-tail convention.
[h, p, stats] = ansaribradley(a, b, 'Tail', 'right');
fprintf('det5 right: p=%.10f  (MATLAB: 0.9996853186)\n', p);
[h, p, stats] = ansaribradley(a, b, 'Tail', 'left');
fprintf('det5 left:  p=%.10f  (MATLAB: 0.0003146814)\n', p);
