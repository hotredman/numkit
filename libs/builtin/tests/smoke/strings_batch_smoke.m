clear

import compat.*

% Strings batch — spec closure 2026-05-09. 14 functions.

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

% num2str default precision is magnitude-aware (MATLAB R2025b), not fixed 5 sig.
fprintf('\n=== num2str magnitude-aware default ===\n');
fprintf('num2str(1000000)   = "%s" (expect 1000000)\n', num2str(1000000));
fprintf('num2str(1000000.5) = "%s" (expect 1000000.5)\n', num2str(1000000.5));
fprintf('num2str(12345.678) = "%s" (expect 12345.678)\n', num2str(12345.678));
fprintf('num2str(pi)        = "%s" (expect 3.1416)\n', num2str(pi));
fprintf('num2str(Inf)="%s" num2str(NaN)="%s"\n', num2str(Inf), num2str(NaN));

% str2double strips commas (thousands sep) + requires whole token. MATLAB R2025b.
fprintf('\n=== str2double commas + consume-all ===\n');
fprintf('str2double("1,234")   = %g (expect 1234)\n', str2double('1,234'));
fprintf('str2double("1,000,000")= %g (expect 1000000)\n', str2double('1,000,000'));
fprintf('str2double("42abc") isnan=%d (expect 1)\n', isnan(str2double('42abc')));
fprintf('str2double("42 7")  isnan=%d (expect 1)\n', isnan(str2double('42 7')));
fprintf('str2double("3.14")    = %g (expect 3.14, regress)\n', str2double('3.14'));
