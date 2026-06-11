clear
import compat.*
% randsample population-vector + weighted forms (row-vector weighted used
% to error "weights length must equal sample-axis size"). Weights with all
% mass on one element make the output deterministic regardless of RNG.
function pr(tag, v)
  fprintf('%s = [', tag);
  for k = 1:numel(v); fprintf(' %g', v(k)); end
  fprintf(' ]\n');
end

pr('randsample([10 20 30],4,true,[0 0 1]) (exp 30 30 30 30)', ...
   randsample([10 20 30], 4, true, [0 0 1]));
pr('randsample([10 20 30],3,true,[1 0 0])  (exp 10 10 10)', ...
   randsample([10 20 30], 3, true, [1 0 0]));

y = randsample([10;20;30], 2, true, [0 1 0]);
fprintf('column population -> size=[%g %g], vals=[%g %g] (exp 2 1 / 20 20)\n', ...
        size(y,1), size(y,2), y(1), y(2));

fprintf('unweighted row randsample([10 20 30],2) numel=%g (exp 2)\n', ...
        numel(randsample([10 20 30], 2)));
