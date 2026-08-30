clear

v = [2 5 3 7 4 6 NaN 8 1 9]';
A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11];

fprintf('default=%g (expect NaN)\n', median(v));
fprintf('omitnan=%g (expect 5)\n', median(v, 'omitnan'));
fprintf('A "all"=%g (expect 6)\n', median(A, 'all'));
fprintf('A [1 2]=%g (expect 6)\n', median(A, [1 2]));
