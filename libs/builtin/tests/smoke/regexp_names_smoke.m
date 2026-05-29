clear
import compat.*

% regexp 'names' — named tokens (?<name>...) collected into a struct.

% Single match → 1x1 struct.
n = regexp('John Smith age 42', ...
           '(?<first>\w+)\s+(?<last>\w+)\s+age\s+(?<age>\d+)', 'names');
fprintf('single : first=%s last=%s age=%s  size %dx%d\n', ...
        n.first, n.last, n.age, size(n,1), size(n,2));   % John Smith 42, 1x1

% Multiple matches → 1xN struct array.
t = regexp('a1 b2 c3', '(?<L>[a-z])(?<D>\d)', 'names');
fprintf('array  : numel=%d  L=%s%s%s  D=%s%s%s\n', ...
        numel(t), t(1).L, t(2).L, t(3).L, t(1).D, t(2).D, t(3).D);  % 3, abc, 123

% Non-participating group (alternation) → empty char.
np = regexp('x', '(?<a>a)|(?<b>x)', 'names');
fprintf('partial: a=[%s] b=[%s]\n', np.a, np.b);                    % [] [x]

% No match → 0x0 struct.
z = regexp('zzz', '(?<L>[a-z])(?<D>\d)', 'names');
fprintf('nomatch: numel=%d size %dx%d\n', numel(z), size(z,1), size(z,2)); % 0, 0x0

% Named group still works as a plain capture group for 'tokens'.
tk = regexp('a1', '(?<L>[a-z])(\d)', 'tokens');
fprintf('tokens : %s %s\n', tk{1}{1}, tk{1}{2});                    % a 1
