% Help System Demo — MATLAB-style Help & Library Catalog
% Demonstrates how to explore Numkit's built-in functions,
% toolboxes, and documentation using help, what, and builtins.

clear;
clc;

fprintf('====================================================\n');
fprintf('  1. Overview of All Help Topics and Toolboxes\n');
fprintf('====================================================\n\n');
help;

fprintf('====================================================\n');
fprintf('  2. Category: Elementary Matrices (elmat)\n');
fprintf('====================================================\n\n');
help elmat;

fprintf('====================================================\n');
fprintf('  3. Category: Elementary Math Functions (elfun)\n');
fprintf('====================================================\n\n');
help elfun;

fprintf('====================================================\n');
fprintf('  4. Category: Matrix Functions & Linear Algebra (matfun)\n');
fprintf('====================================================\n\n');
help matfun;

fprintf('====================================================\n');
fprintf('  5. Toolbox: Image Processing (image)\n');
fprintf('====================================================\n\n');
help image;

fprintf('====================================================\n');
fprintf('  6. Toolbox: Signal Processing (signal)\n');
fprintf('====================================================\n\n');
help signal;

fprintf('====================================================\n');
fprintf('  7. Function Documentation (help <func>)\n');
fprintf('====================================================\n\n');
help svd;
help bwareaopen;
help butter;

fprintf('====================================================\n');
fprintf('  8. Programmatic Introspection with what() & builtins()\n');
fprintf('====================================================\n\n');
w = what('elmat');
fprintf('Total functions in elmat: %d\n', numel(w.m));
fprintf('First 5 functions: %s, %s, %s, %s, %s\n', w.m{1}, w.m{2}, w.m{3}, w.m{4}, w.m{5});

allFuncs = builtins();
fprintf('Total registered functions in Numkit: %d\n', numel(allFuncs));
