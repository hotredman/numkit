clear

% risetime / falltime: [R, LT, UT, LL, UL] — duration, lower(10%) and
% upper(90%) crossing times, lower/upper reference levels.
[R, LT, UT, LL, UL] = risetime([0 0 0 1 1 1 1], 4);
fprintf('risetime sharp edge: R=%.4f LT=%.4f UT=%.4f LL=%.4f UL=%.4f\n', ...
        R, LT, UT, LL, UL);
fprintf('  (expect R=0.1980 LT=0.5260 UT=0.7240 LL=0.1040 UL=0.8960)\n');

[F, LTf, UTf] = falltime([1 1 1 0 0 0 0], 4);
fprintf('falltime sharp edge: F=%.4f LT=%.4f UT=%.4f\n', F, LTf, UTf);
fprintf('  (expect F=0.1980 LT=0.7240 UT=0.5260 - lower crosses last)\n');

% Gradual ramp (already correct before the fix).
fprintf('risetime ramp = %.4f  (expect 3.1680)\n', ...
        risetime([0 0 0.25 0.5 0.75 1 1], 1));
