% assignin — Setter pattern that writes into caller's workspace
%
% `assignin('caller', name, value)` writes `value` into the workspace
% of the function that called the function containing this assignin.
% Used when a helper needs to inject named values back into its
% caller without returning them through the normal return-value
% interface.
%
% Three workspaces:
%   'base'   — the top-level / REPL workspace.
%   'caller' — the workspace of the function that called the
%              function containing this `assignin` call. Throws if
%              invoked from the base workspace itself (no caller
%              above it).
%
% MATLAB-parity. Available without `import compat.*` — assignin
% lives in core (it's a workspace-introspection builtin, like clear).

clear all

% --- Setter helper that writes into the calling function ---
function setup_constants(prefix)
    % Build three names with `prefix` and assign integers into the
    % caller. The caller's compiler must see those names statically
    % (e.g. via subsequent `disp` or arithmetic) for register
    % write-through to surface them — that's why `f` below
    % references the names directly.
    assignin('caller', [prefix, '_one'],   1);
    assignin('caller', [prefix, '_two'],   2);
    assignin('caller', [prefix, '_three'], 3);
end

function r = f()
    setup_constants('cfg');
    % The three cfg_* names were just set in *this* function's
    % workspace by assignin('caller', ...). Read them as ordinary
    % locals.
    disp('Constants visible in the caller:')
    disp(['  cfg_one   = ', num2str(cfg_one)])
    disp(['  cfg_two   = ', num2str(cfg_two)])
    disp(['  cfg_three = ', num2str(cfg_three)])
    r = cfg_one + cfg_two + cfg_three;
end

total = f();
fprintf('Total: %d\n', total)

% --- Setting from base workspace ---
% From the top-level (base) workspace, 'caller' is invalid (there's
% no enclosing function above us). 'base' explicitly targets the
% top-level workspace.
assignin('base', 'top_level_marker', 42);
disp(['top_level_marker = ', num2str(top_level_marker)])

% --- This would throw: 'caller' from base ---
% Uncomment to see the error:
% assignin('caller', 'x', 1);   % "'caller' is not valid in the base workspace"
