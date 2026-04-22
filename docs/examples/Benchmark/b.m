function b(name, fn, R)
% b(name, fn, R) — bench helper for benchmark_parity.m.
%
% Runs fn() R times under tic/toc and prints "label  time ms".
% On error (missing builtin in the host runtime, etc.) prints
% "label  skipped (reason)" so the caller script can keep going.
%
% Lives as its own .m file so Octave 11 picks it up alongside the
% calling script — Octave's local-function-in-script support is
% spottier than MATLAB's.
    try
        tic;
        for r = 1:R
            fn();
        end
        t = toc;
        fprintf('  %-26s %8.2f ms\n', name, t * 1000);
    catch err
        fprintf('  %-26s skipped (%s)\n', name, err.message);
    end
end
