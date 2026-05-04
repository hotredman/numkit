clear

import compat.*

% mmgradm — morphological gradient = imdilate − imerode.

I = uint8([10 20 30 40 50; 15 25 35 45 55; 20 30 40 50 60]);

fprintf('--- default cross SE ---\n');
G = mmgradm(I);
disp(double(G));
fprintf('  expect: max-min over plus-shaped neighborhood\n\n');

% Cross-check: equals manual imdilate - imerode with default SE.
SE = [0 1 0; 1 1 1; 0 1 0];
manual = imdilate(I, SE) - imerode(I, SE);
fprintf('matches imdilate−imerode? %d\n', isequal(G, manual));

fprintf('\n--- external (half) gradient: mmgradm(I, SE, []) ---\n');
ext = mmgradm(I, SE, []);
fprintf('matches imdilate? %d\n', isequal(ext, imdilate(I, SE)));

fprintf('\n--- logical input ---\n');
L = logical([0 1 0 0; 1 1 1 0; 0 1 0 0]);
GL = mmgradm(L);
fprintf('class = %s\n', class(GL));
disp(double(GL));
fprintf('  (boundary of plus shape; dilated & ~eroded)\n');
