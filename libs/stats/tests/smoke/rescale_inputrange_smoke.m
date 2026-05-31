clear
import compat.*
% rescale 'InputMin'/'InputMax' Name-Value (was unsupported -> "Cannot
% convert char to scalar"). Values clamp to the input range. vs MATLAB.
function pr(tag, v)
  fprintf('%s = [', tag);
  for k = 1:numel(v); fprintf(' %.6g', v(k)); end
  fprintf(' ]\n');
end
x = [1 2 3 4 5];

pr('InputMin 2 InputMax 4   (exp 0 0 .5 1 1)',   rescale(x, 'InputMin', 2, 'InputMax', 4));
pr('0 10 + InputMin2 Max4   (exp 0 0 5 10 10)',  rescale(x, 0, 10, 'InputMin', 2, 'InputMax', 4));
pr('InputMax 3              (exp 0 .5 1 1 1)',   rescale(x, 'InputMax', 3));
pr('InputMin 0              (exp .2 .4 .6 .8 1)', rescale(x, 'InputMin', 0));
pr('plain -1 1              (exp -1 -.5 0 .5 1)', rescale(x, -1, 1));
