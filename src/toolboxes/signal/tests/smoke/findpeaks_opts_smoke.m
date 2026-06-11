clear
import compat.*
% findpeaks Name-Value options (MATLAB R2025b). Peaks: 1@2 2@4 3@6 2@8 1@10.
x = [0 1 0 2 0 3 0 2 0 1 0];

function pr(tag, v)
  fprintf('%s = [', tag);
  for k = 1:numel(v); fprintf(' %g', v(k)); end
  fprintf(' ]\n');
end

pr('default pk      (exp 1 2 3 2 1)', findpeaks(x));
pr('MinPeakHeight 2 (exp 3)        ', findpeaks(x, 'MinPeakHeight', 2));
pr('Threshold 2     (exp 2 3 2)    ', findpeaks(x, 'Threshold', 2));

[p, l] = findpeaks(x, 'SortStr', 'descend');
pr('SortStr desc pk (exp 3 2 2 1 1)', p);
pr('SortStr desc loc(exp 6 4 8 2 10)', l);

[p2, l2] = findpeaks(x, 'MinPeakDistance', 3);
pr('MinPeakDist 3 pk(exp 1 3 1)    ', p2);
pr('MinPeakDist 3 loc(exp 2 6 10)  ', l2);

[p3, l3] = findpeaks(x, 'NPeaks', 2, 'SortStr', 'descend');
pr('NPeaks2+desc pk (exp 3 2)      ', p3);
pr('NPeaks2+desc loc(exp 6 4)      ', l3);

% Location form findpeaks(Y, X): locations come from X.
X = 0:0.5:5;
[~, l4] = findpeaks(x, X);
pr('findpeaks(Y,X) loc(exp .5 1.5 2.5 3.5 4.5)', l4);
