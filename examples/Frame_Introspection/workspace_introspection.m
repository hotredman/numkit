% Workspace introspection — Combining assignin / evalin / inputname
%
% A small "named-config" pattern: a helper function builds a
% workspace's worth of named values from a struct, using
%   assignin    to write each named value into the caller,
%   inputname   to refer to the caller's source variable in errors,
%   evalin      to verify what got written.
%
% Available without `import compat.*` — all four are core builtins.

clear all

% --- Helper: expand a struct's fields into named locals in caller ---
function expand_struct(s)
    name = inputname(1);
    if isempty(name)
        name = '<expression>';
    end
    if ~isstruct(s)
        error('expand_struct: %s must be a struct', name);
    end
    fields = fieldnames(s);
    fprintf('Expanding "%s" (%d fields) into caller:\n', name, length(fields));
    for i = 1:length(fields)
        fld = fields{i};
        val = s.(fld);
        assignin('caller', fld, val);
        fprintf('  %s = ', fld);
        disp(val);
    end
end

% --- Helper: snapshot two named values from caller into a struct ---
% Uses inputname(k) to capture the source variable names, evalin to
% read their values from the caller's scope. Two-arg variant — our
% engine doesn't support varargin yet, so the helper has a fixed
% number of inputs.
function snap = snapshot2(a, b)
    snap = struct();
    n1 = inputname(1);
    n2 = inputname(2);
    if isempty(n1) || isempty(n2)
        error('snapshot2: both args must be bare identifiers');
    end
    snap.(n1) = evalin('caller', n1);
    snap.(n2) = evalin('caller', n2);
end

% --- Demo ---
function demo()
    % Build a config struct.
    cfg.fs        = 44100;
    cfg.duration  = 2.5;
    cfg.amplitude = 0.8;

    % Expand its fields into this function's workspace as named
    % locals (each becomes an ordinary variable).
    expand_struct(cfg);

    % Now fs, duration, amplitude are local variables.
    fprintf('\nUsing the expanded values:\n');
    n_samples = round(fs * duration);
    fprintf('  n_samples = round(%g * %g) = %d\n', fs, duration, n_samples);
    fprintf('  amplitude = %g\n', amplitude);

    % Snapshot a subset back into a struct, recording the source names.
    snap = snapshot2(fs, n_samples);
    fprintf('\nSnapshot taken:\n');
    fprintf('  snap.fs        = %g\n', snap.fs);
    fprintf('  snap.n_samples = %d\n', snap.n_samples);
end

demo();

% --- demo()'s locals do NOT leak into base ---
% After demo returns, fs / duration / amplitude / n_samples are gone.
fprintf('\nBack at top level. Locals from demo() are out of scope:\n');
names = {'fs', 'duration', 'amplitude', 'n_samples'};
for i = 1:length(names)
    nm = names{i};
    if exist(nm, 'var')
        fprintf('  UNEXPECTED: %s leaked into base\n', nm);
    else
        fprintf('  %s — not in base (correct)\n', nm);
    end
end
