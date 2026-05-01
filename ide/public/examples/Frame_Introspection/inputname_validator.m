% inputname — Read the source-text name of an argument
%
% `inputname(k)` returns the variable name as written at the call
% site for the k-th input argument. If that arg was a literal,
% expression, or any non-bare-identifier — returns "" (empty).
%
% Used to produce error messages that reference the *caller's*
% variable name rather than a parameter name.
%
% Available without `import compat.*` — core builtin.

clear all

% --- Validator with caller-sourced names in the error message ---
function check_in_range(x, lo, hi)
    name = inputname(1);
    if isempty(name)
        name = '<expression>';
    end
    if x < lo || x > hi
        error('%s = %g is outside [%g, %g]', name, x, lo, hi);
    end
    fprintf('  OK: %s = %g is in [%g, %g]\n', name, x, lo, hi);
end

% Bare-identifier arg → name shows up in messages.
temperature = 23.5;
pressure = 1.05;

check_in_range(temperature, 0, 100);   % "temperature" appears in messages
check_in_range(pressure, 0, 10);

% Literal / expression args have no source name.
check_in_range(50, 0, 100);            % name = "" → "<expression>"
check_in_range(temperature * 2, 0, 100);

% Out-of-range — error uses the caller's variable name.
fprintf('\nIntentionally triggering an error:\n');
try
    rogue_val = 1000;
    check_in_range(rogue_val, 0, 100);
catch err
    fprintf('  Caught: %s\n', err.message);
end

% --- All input names at once ---
function show_input_names(a, b, c)
    fprintf('\nInside show_input_names:\n');
    for k = 1:nargin
        n = inputname(k);
        if isempty(n)
            n = '<expression>';
        end
        fprintf('  arg %d was passed as: %s\n', k, n);
    end
end

x = 1; y = 2; z = 3;
show_input_names(x, y, z);
show_input_names(10, x + y, sin(0));     % literal, expr, expr

% --- Function handle preserves arg names too ---
% inputname works through `@` indirection — the call site of the
% handle invocation is what's recorded.
fn = @show_input_names;
fprintf('\nInvoked through handle:\n');
fn(x, y, z);
