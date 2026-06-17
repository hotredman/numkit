clear
import compat.*
% regexprep(...,'once'): replace only the FIRST match of each pattern.
% DEEP-PROBE 2026-05-31: numkit parsed only 'ignorecase' and silently
% ignored 'once', replacing ALL matches. Values pinned vs MATLAB R2025b.

fprintf('[%s]  (expect Xbc)\n',           regexprep('abc','\w','X','once'));
fprintf('[%s]  (expect a#b2c3)\n',         regexprep('a1b2c3','\d','#','once'));
fprintf('[%s]  (expect ZaAa)\n',           regexprep('AaAa','a','Z','once','ignorecase'));
fprintf('[%s]  (expect <hello> world)\n',  regexprep('hello world','(\w+)','<$1>','once'));
fprintf('[%s]  (expect abc, no match)\n',  regexprep('abc','x','Y','once'));

% Default (no 'once') still replaces every occurrence.
fprintf('[%s]  (expect XXX)\n',            regexprep('abc','\w','X'));

% Cell-string input: 'once' applies element-wise.
r = regexprep({'aa','bb'},'\w','#','once');
fprintf('cell once = [%s %s]  (expect #a #b)\n', r{1}, r{2});
