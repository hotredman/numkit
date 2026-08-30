clear

% downsample/upsample phase argument (was silently ignored).
function pr(tag, v)
  fprintf('%s = [', tag);
  for k = 1:numel(v); fprintf(' %g', v(k)); end
  fprintf(' ]\n');
end

pr('downsample(1:10,3)   (exp 1 4 7 10)', downsample(1:10, 3));
pr('downsample(1:10,3,1) (exp 2 5 8)',    downsample(1:10, 3, 1));
pr('downsample(1:10,3,2) (exp 3 6 9)',    downsample(1:10, 3, 2));
pr('upsample(1:3,3)      (exp 1 0 0 2 0 0 3 0 0)', upsample(1:3, 3));
pr('upsample(1:3,3,1)    (exp 0 1 0 0 2 0 0 3 0)', upsample(1:3, 3, 1));
pr('upsample(1:3,3,2)    (exp 0 0 1 0 0 2 0 0 3)', upsample(1:3, 3, 2));
