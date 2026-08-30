clear

A = [1 3 2 5 4 6 NaN 8 7 10]';

fprintf('=== cummax ===\n');
disp(cummax(A)');                              fprintf('  expect [1 3 3 5 5 6 6 8 8 10]\n');
disp(cummax(A, ''reverse'')');                 fprintf('  expect [10 ...]\n');
disp(cummax(A, ''includenan'')');              fprintf('  expect NaN from pos 7\n');
disp(cummax(A, ''reverse'', ''includenan'')'); fprintf('  expect NaN before pos 8\n');

fprintf('\n=== cummin ===\n');
disp(cummin(A)');
disp(cummin(A, ''reverse'')');
