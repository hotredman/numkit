clear

% whitepoint — CIE reference illuminant XYZ tristimulus.

fprintf('--- default (icc) ---\n');
disp(whitepoint());
fprintf('  expect [0.96420288 1 0.82489014]\n\n');

fprintf('--- d65 ---\n');
disp(whitepoint('d65'));
fprintf('  expect [0.95047 1 1.08883]\n\n');

fprintf('--- d50 ---\n');
disp(whitepoint('d50'));
fprintf('  expect [0.96419866 1 0.82511648]\n\n');

fprintf('--- a (Tungsten) ---\n');
disp(whitepoint('a'));
fprintf('  expect [1.0985 1 0.3558]\n\n');

fprintf('--- e (Equal-energy) ---\n');
disp(whitepoint('e'));
fprintf('  expect [1 1 1]\n\n');

fprintf('--- case insensitive ---\n');
disp(whitepoint('D65'));
fprintf('  expect [0.95047 1 1.08883]\n');
