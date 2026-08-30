clear

% maxk / mink now support 'ComparisonMethod','abs', which ranks elements by
% MAGNITUDE |x| (returning the ORIGINAL signed values, not their absolute
% values); ties break by ascending original index. numkit previously threw
% "ComparisonMethod='abs' not yet supported". The default ('real'/'auto') is
% unchanged.

x = [3 -7 2 -5 1 -9 4];

fprintf('--- maxk by |x| (largest magnitude first) ---\n');
[v, i] = maxk(x, 3, 'ComparisonMethod', 'abs');
fprintf('values :'); fprintf(' %d', v); fprintf('   (expect -9 -7 -5)\n');
fprintf('indices:'); fprintf(' %d', i); fprintf('   (expect  6  2  4)\n');

fprintf('--- mink by |x| (smallest magnitude first) ---\n');
[v2, i2] = mink(x, 3, 'ComparisonMethod', 'abs');
fprintf('values :'); fprintf(' %d', v2); fprintf('   (expect 1 2 3)\n');
fprintf('indices:'); fprintf(' %d', i2); fprintf('   (expect 5 3 1)\n');

fprintf('--- ties: |-3| == |3| -> lower index first ---\n');
[tv, ti] = maxk([-3 3 1 -1], 2, 'ComparisonMethod', 'abs');
fprintf('values :'); fprintf(' %d', tv); fprintf('   (expect -3 3)\n');
fprintf('indices:'); fprintf(' %d', ti); fprintf('   (expect  1 2)\n');

fprintf('--- default ''real'' unaffected ---\n');
r = maxk(x, 3, 'ComparisonMethod', 'real');
fprintf('values :'); fprintf(' %d', r); fprintf('   (expect 4 3 2)\n');

fprintf('--- matrix: maxk by |x| down each column ---\n');
M = maxk([1 -8; -3 2; 5 -1], 2, 1, 'ComparisonMethod', 'abs');
fprintf('col1:'); fprintf(' %d', M(:,1)); fprintf('   col2:'); fprintf(' %d', M(:,2));
fprintf('   (expect col1 5 -3, col2 -8 2)\n');
