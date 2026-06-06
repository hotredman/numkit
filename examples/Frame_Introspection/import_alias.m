% import — All four forms (single, wildcard, alias, multi-arg)
%
% `import` is a regular command-style builtin. It pushes a
% namespace lookup hint into the current scope so subsequent calls
% resolve short names against that namespace.
%
%   import a.b.c            single symbol         (calls c → a.b.c)
%   import a.b.*            wildcard              (any leaf in a.b.*)
%   import a.b.* x.y.*      multi-arg             (multiple wildcards)
%   import a.b as alias     prefix substitution   (alias.foo → a.b.foo)
%
% Active from the import line to the end of the enclosing scope
% (function body / script). Inside a user function, imports are
% scope-local — they vanish when the function returns.
%
% This example does NOT need `import compat.*` because it
% demonstrates `import` itself.

clear all

% --- 1. Single-symbol import ---
import signal.transforms.fft;
y1 = fft([1 1 1 1]);
fprintf('Single-symbol: fft([1 1 1 1]) = ');
disp(y1)

% --- 2. Wildcard ---
% Pull every leaf under signal.transforms into the workspace.
import signal.transforms.*;
y2 = ifft(y1);
fprintf('Wildcard ifft: ');
disp(real(y2))   % should be [1 1 1 1]

% --- 3. Multi-arg (multiple wildcards in one statement) ---
import signal.windows.* signal.filter_design.*;
w = hamming(8);
fprintf('Hamming window length 8: ');
disp(w')

% --- 4. Alias prefix substitution ---
import signal.transforms as tr;
y3 = tr.fft([2 0 0 0]);     % expands to signal.transforms.fft(...)
fprintf('Via alias tr.fft([2 0 0 0]): ');
disp(y3)

% --- Function-style equivalent ---
% Every command-style form has a function-style equivalent. The
% builtin parses the string args itself.
import('signal.measurements.*');     % wildcard
y4 = rms([1 2 3 4 5]);
fprintf('After import(''signal.measurements.*''): rms = %g\n', y4);

% --- Function-local import does NOT leak to caller ---
function inner_call()
    import compat.*;            % flatten the world inside this fn
    a = std([1 2 3 4 5]);
    fprintf('  (inside inner_call) std = %g\n', a);
end

inner_call();
% After return: at this point compat.* is NOT active anymore.
% `std` would not resolve unless we import it in this scope too.
fprintf('\nTop-level still has signal.transforms via wildcard above:\n');
fprintf('  fft([1 0 0 0]) = ');
disp(real(fft([1 0 0 0])));
