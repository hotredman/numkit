% eval — Run a string as code in the current workspace
%
% `eval(string)` parses and executes `string` as if it were typed
% inline at the eval call site. Variables defined by the string
% become visible to the surrounding code; the expression value (when
% the string is a single expression) is returned.
%
% Scope rule: eval inside a function runs in *that function's*
% workspace, not in the global workspace. Variables defined by eval
% are local to the function and disappear when it returns. (Set the
% `NUMKIT_LEGACY_EVAL_SCOPE=1` env var to revert to the pre-2026-05
% behaviour where eval/run leaked to base.)
%
% Available without `import compat.*` — core builtin.

clear all

% --- Computed expressions ---
% Build the source string at runtime and evaluate.
operations = {'+', '-', '*', '/'};
a = 12;
b = 4;
fprintf('Operations on a=%d, b=%d:\n', a, b);
for i = 1:length(operations)
    op = operations{i};
    expr = sprintf('a %s b', op);
    result = eval(expr);
    fprintf('  %s  = %g\n', expr, result);
end

% --- Dispatch table by name ---
% Each function name lives as a string; eval calls it.
function dispatch_demo()
    fns = {'sin', 'cos', 'tan', 'exp'};
    x = 0.5;
    fprintf('\nFunction values at x = %g:\n', x);
    for i = 1:length(fns)
        fname = fns{i};
        v = eval([fname, '(x)']);
        fprintf('  %s(x) = %.4f\n', fname, v);
    end
end

dispatch_demo();

% --- eval defines variable scoped to its caller ---
function r = eval_scope_demo()
    eval('local_v = 100;');
    % local_v is now in this function's scope. Top-level workspace
    % won't see it after we return.
    r = local_v;
end

leaked = eval_scope_demo();
fprintf('\nValue from inside function: %d\n', leaked);
% Top-level should NOT have local_v defined:
if exist('local_v', 'var')
    disp('UNEXPECTED: local_v leaked into base');
else
    disp('Confirmed: local_v stayed inside function (no leak)');
end

% --- eval at top-level persists (REPL semantics) ---
eval('persisted = 7;');
fprintf('Defined at top-level via eval: persisted = %d\n', persisted);
