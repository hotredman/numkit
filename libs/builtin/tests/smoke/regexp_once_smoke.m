clear
import compat.*
% regexp(...,'once'): match only the first occurrence and return the
% scalarised form. DEEP-PROBE 2026-05-31: numkit used to throw
% "unknown option 'once'". Values pinned vs MATLAB R2025b.

s = 'a1b2c3';
fprintf('start once = %d  (expect 2)\n', regexp(s,'\d','once'));
fprintf('end   once = %d  (expect 2)\n', regexp(s,'\d','end','once'));

m = regexp(s,'\d','match','once');
fprintf('match once = [%s]  ischar=%d  (expect [1] 1)\n', m, ischar(m));

t = regexp(s,'(\w)(\d)','tokens','once');
fprintf('tokens once = {%s, %s}  numel=%d  (expect {a, 1} 2)\n', t{1}, t{2}, numel(t));

sp = regexp('a,b,c',',','split','once');
fprintf('split once = {%s, %s}  numel=%d  (expect {a, b,c} 2)\n', sp{1}, sp{2}, numel(sp));

% No match -> empty (start [], match '').
fprintf('no-match start isempty = %d  (expect 1)\n', isempty(regexp('xyz','\d','once')));
mm = regexp('xyz','\d','match','once');
fprintf('no-match match isempty = %d  ischar = %d  (expect 1 1)\n', isempty(mm), ischar(mm));
