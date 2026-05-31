clear

import compat.*

% sprintf/fprintf %s with the string type. Bug fixed 2026-05-30: %s only
% accepted char arrays, so a string scalar printed nothing
% (fprintf('%s', "hi") gave empty). Now %s prints string scalars and cycles
% the format over a string array's elements. vs MATLAB R2025b.

fprintf('=== string scalar ===\n');
fprintf('fprintf %%s "hello"  = [%s] (expect [hello])\n', "hello");
s = "world";
fprintf('var string          = [%s] (expect [world])\n', s);
fprintf('sprintf result      = [%s] (expect [ab-cd])\n', sprintf('%s-%s', "ab", "cd"));

fprintf('\n=== string array cycles the format ===\n');
fprintf('one-spec over array  = [%s] (expect [xyz])\n', sprintf('%s', ["x" "y" "z"]));
fprintf('%%s per element:\n');
fprintf('  elem=[%s]\n', ["ab" "cd"]);

fprintf('\n=== mixed string + numeric ===\n');
fprintf('mix                  = [%s] (expect [k=7])\n', sprintf('%s=%d', "k", 7));

fprintf('\n=== char still works (regression) ===\n');
fprintf('char arg             = [%s] (expect [chars])\n', 'chars');
