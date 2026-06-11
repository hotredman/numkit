clear
import compat.*
% histc — legacy bin counts (length(edges) bins, last = exact-equal to last edge).
function pr(tag, v)
  fprintf('%s = [', tag);
  for k = 1:numel(v); fprintf(' %g', v(k)); end
  fprintf(' ]\n');
end
pr('histc([1 2 2 3 5],[0 2 4 6]) (exp 1 3 1 0)', histc([1 2 2 3 5],[0 2 4 6]));
[n, bin] = histc([1 2 2 3 5],[0 2 4 6]);
pr('bin index (exp 1 2 2 2 3)', bin);
H = histc([1 5; 2 6; 3 7],[0 4 8]);
fprintf('matrix col-wise: size=[%g %g], col1=[%g %g %g], col2=[%g %g %g]\n', ...
        size(H,1), size(H,2), H(1,1),H(2,1),H(3,1), H(1,2),H(2,2),H(3,2));
fprintf('  expect 3 2 / [3 0 0] / [0 3 0]\n');
