clear

% isbw / isgray / isind / isrgb — image-type predicates.

fprintf('--- isbw ---\n');
fprintf('isbw(logical([0 1; 1 0]))            = %d (expect 1)\n', isbw(logical([0 1; 1 0])));
fprintf('isbw([0 1; 1 0])                     = %d (expect 0; default ''logical'')\n', isbw([0 1; 1 0]));
fprintf('isbw([0 1; 1 0], ''non-logical'')     = %d (expect 1)\n', isbw([0 1; 1 0], 'non-logical'));
fprintf('isbw([0 0.5; 1 0], ''non-logical'')   = %d (expect 0; 0.5 not in {0,1})\n', isbw([0 0.5; 1 0], 'non-logical'));
fprintf('isbw([0 NaN 1], ''non-logical'')      = %d (expect 0; NaN)\n', isbw([0 NaN 1], 'non-logical'));

fprintf('\n--- isgray ---\n');
fprintf('isgray([0 0 1; 1 0 1])    = %d (expect 1)\n', isgray([0 0 1; 1 0 1]));
fprintf('isgray(zeros(3))          = %d (expect 1)\n', isgray(zeros(3)));
fprintf('isgray(rand(10) + 1)      = %d (expect 0; out of [0,1])\n', isgray(rand(10) + 1));
fprintf('isgray(uint8(zeros(5)))   = %d (expect 1)\n', isgray(uint8(zeros(5))));
fprintf('isgray(rand(5,5,3))       = %d (expect 0; 3-channel)\n', isgray(rand(5,5,3)));
A = NaN * ones(5);
fprintf('isgray(all-NaN)           = %d (expect 0; all NaN)\n', isgray(A));

fprintf('\n--- isind ---\n');
fprintf('isind(1:10)               = %d (expect 1)\n', isind(1:10));
fprintf('isind(0:10)               = %d (expect 0; contains 0)\n', isind(0:10));
fprintf('isind([1.3 2.4])          = %d (expect 0; not integer)\n', isind([1.3 2.4]));
fprintf('isind(uint8([1 2; 3 4]))  = %d (expect 1)\n', isind(uint8([1 2; 3 4])));

fprintf('\n--- isrgb ---\n');
fprintf('isrgb(rand(5,5,3))        = %d (expect 1)\n', isrgb(rand(5,5,3)));
fprintf('isrgb(rand(5,5))          = %d (expect 0)\n', isrgb(rand(5,5)));
fprintf('isrgb(ones(5,5,3) + eps)  = %d (expect 0; >1)\n', isrgb(ones(5,5,3) + eps));
fprintf('isrgb(uint8(zeros(2,2,3)))= %d (expect 1)\n', isrgb(uint8(zeros(2,2,3))));
