clear

import compat.*

% Strings batch — audit ТЗ closure 2026-05-09. 14 functions.

fprintf('lower("Hello")     = "%s"\n',     lower("Hello"));
fprintf('upper("xyz")       = "%s"\n',     upper("xyz"));
fprintf('strtrim("  ab  ")  = "%s"\n',     strtrim("  ab  "));
fprintf('deblank("abc   ")  = "%s"\n',     deblank("abc   "));
fprintf('blanks(5) length   = %d\n',       strlength(blanks(5)));
fprintf('strlength("hello") = %d\n',       strlength("hello"));
fprintf('strrep("ab","b","X") = "%s"\n',   strrep("ab","b","X"));
fprintf('strfind("hello","l") = '); disp(strfind("hello","l"));
fprintf('contains("hello","ll") = %d\n',   contains("hello","ll"));
fprintf('startsWith("hello","he") = %d\n', startsWith("hello","he"));
fprintf('endsWith("hello","lo") = %d\n',   endsWith("hello","lo"));
fprintf('strcat("a","b","c")  = "%s"\n',   strcat("a","b","c"));
parts = strsplit("a b c");
fprintf('strsplit("a b c") = %d parts\n',  numel(parts));
[t, rem] = strtok("hello world");
fprintf('strtok t="%s" rem="%s"\n', t, rem);
