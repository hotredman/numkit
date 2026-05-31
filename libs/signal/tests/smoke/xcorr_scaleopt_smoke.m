clear
import compat.*
% xcorr scaleopt (was accepted-and-ignored -> raw correlation) + maxlag.
function pr(tag, v)
  fprintf('%s = [', tag);
  for k = 1:numel(v); fprintf(' %.6g', v(k)); end
  fprintf(' ]\n');
end
x = [1 2 3];

pr('none     (exp 3 8 14 8 3)',                  xcorr(x, x));
pr('coeff    (exp .2143 .5714 1 .5714 .2143)',   xcorr(x, x, 'coeff'));
pr('biased   (exp 1 2.6667 4.6667 2.6667 1)',    xcorr(x, x, 'biased'));
pr('unbiased (exp 3 4 4.6667 4 3)',              xcorr(x, x, 'unbiased'));

[c, l] = xcorr(x, x, 1);
pr('maxlag1 c    (exp 8 14 8)',  c);
pr('maxlag1 lags (exp -1 0 1)',  l);

pr('maxlag1+coeff (exp .5714 1 .5714)', xcorr(x, x, 1, 'coeff'));
