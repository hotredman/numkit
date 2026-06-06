% compute_stats.m — sibling helper called from main.m
%
% Returns a struct with several summary statistics. Pulls toolbox
% functions in via `import compat.*` so mean / std / median work
% by short name. The import is scope-local to this function — it
% does not leak into main.m.

function s = compute_stats(x)
    import compat.*;

    s = struct();
    s.mean   = mean(x);
    s.median = median(x);
    s.std    = std(x);
    s.min    = min(x);
    s.max    = max(x);
    s.range  = s.max - s.min;
end
