clear
import compat.*
% bwdist distance-metric option (was silently ignored -> always Euclidean).
BW = logical([0 0 0 0; 0 1 0 0; 0 0 0 0; 0 0 0 1]);

function prm(tag, D)
  fprintf('%s:\n', tag);
  for r = 1:size(D,1)
    fprintf('  ');
    for c = 1:size(D,2); fprintf('%7.4f ', D(r,c)); end
    fprintf('\n');
  end
end

prm('euclidean       (row1 ~ 1.41 1 1.41 2.24)', bwdist(BW, 'euclidean'));
prm('cityblock       (row1 = 2 1 2 3)',          bwdist(BW, 'cityblock'));
prm('chessboard      (row1 = 1 1 1 2)',          bwdist(BW, 'chessboard'));
prm('quasi-euclidean (row1 ~ 1.41 1 1.41 2.41)', bwdist(BW, 'quasi-euclidean'));
