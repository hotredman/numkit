clear
import compat.*
% imhist 2nd output x (bin locations) spans the input CLASS display range
% (was always [0,1]). Counts are unchanged. vs MATLAB R2025b.
function pr(tag, v)
  fprintf('%s = [', tag);
  for k = 1:numel(v); fprintf(' %g', v(k)); end
  fprintf(' ]\n');
end

[c, x] = imhist(uint8([0 64 128 192 255]), 4);
pr('uint8 n=4 counts (exp 1 1 2 1)',   c);
pr('uint8 n=4 x      (exp 0 85 170 255)', x);

[~, xd] = imhist([0 0.25 0.5 0.75 1], 4);
pr('double n=4 x     (exp 0 .3333 .6667 1)', xd);

[~, xu] = imhist(uint16([0 32768 65535]), 3);
pr('uint16 n=3 x     (exp 0 32767.5 65535)', xu);
