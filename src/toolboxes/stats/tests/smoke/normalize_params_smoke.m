clear

% normalize method PARAMETER (range bounds / norm-p / scale ref / center
% ref) — was parsed-and-ignored, so each method used its default. vs MATLAB.
function pr(tag, v)
  fprintf('%s = [', tag);
  for k = 1:numel(v); fprintf(' %.6g', v(k)); end
  fprintf(' ]\n');
end
x = [1 2 3 4 5];

pr('range [0 10]   (exp 0 2.5 5 7.5 10)',   normalize(x, 'range', [0 10]));
pr('range [-1 1]   (exp -1 -.5 0 .5 1)',     normalize(x, 'range', [-1 1]));
pr('norm 1         (exp .0667 .1333 .2 .2667 .3333)', normalize(x, 'norm', 1));
pr('norm Inf       (exp .2 .4 .6 .8 1)',     normalize(x, 'norm', Inf));
pr('scale first    (exp 1 2 3 4 5)',         normalize(x, 'scale', 'first'));
pr('center median  (exp -2 -1 0 1 2)',       normalize(x, 'center', 'median'));

% DEEP-PROBE 2026-05-31: 'zscore','robust' (median + raw MAD) was ignored ->
% fell back to mean/std. median([1 2 3 4 100])=3, MAD=1 -> [-2 -1 0 1 97].
xr = [1 2 3 4 100];
pr('zscore robust (exp -2 -1 0 1 97)',        normalize(xr, 'zscore', 'robust'));
pr('zscore std    (exp -.481 -.459 -.436 -.413 1.788)', normalize(xr, 'zscore', 'std'));
