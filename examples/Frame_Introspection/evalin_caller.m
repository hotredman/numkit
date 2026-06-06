% evalin — Read and write caller's workspace via dynamic code
%
% `evalin(workspace, code)` runs `code` as if it were typed in the
% target workspace ('base' or 'caller'). Variables defined by the
% code land in that workspace; expressions read variables from there.
%
% Powerful for diagnostic helpers and interactive utilities, but
% sharp — code runs in the caller's scope, so any side effect
% (variable assignment, import, etc.) lands there, not in the
% helper's own scope.
%
% Available without `import compat.*` — evalin is a core builtin.

clear all

% --- Helper that reads a named variable from caller and validates it ---
function check_positive(name)
    val = evalin('caller', name);
    if val <= 0
        error('%s must be positive, got %g', name, val);
    end
    fprintf('  %s = %g (OK)\n', name, val);
end

function f()
    width  = 100;
    height =  50;
    depth  =  25;
    disp('Validating dimensions in f():')
    check_positive('width');
    check_positive('height');
    check_positive('depth');
end

f();

% --- Helper that updates a named variable in caller ---
function bump_counter(name)
    % Read, increment, write back. Each line is a separate evalin
    % call into the caller's scope — they share the same scope so
    % they see each other's effects.
    current = evalin('caller', name);
    evalin('caller', sprintf('%s = %d;', name, current + 1));
end

function counter_demo()
    n = 0;
    for i = 1:5
        bump_counter('n');
    end
    fprintf('After 5 bumps: n = %d\n', n);
end

counter_demo();

% --- evalin('base') from inside a function ---
% Always targets the top-level, regardless of where it's called from.
function leak_to_base(value)
    evalin('base', sprintf('top_level_result = %d;', value));
end

leak_to_base(99);
fprintf('Leaked to base: top_level_result = %d\n', top_level_result);

% --- evalin returns the expression value ---
% When the eval'd code is a value-expression, evalin returns its
% value. Inside `compute_sum` we read three named locals from the
% function that called us (`outer_demo`).
function s = compute_sum()
    s = evalin('caller', 'a + b + c');
end

function outer_demo()
    a = 10;
    b = 20;
    c = 30;
    total = compute_sum();
    fprintf('Sum read by inner via evalin(''caller''): %d\n', total);
end

outer_demo();
