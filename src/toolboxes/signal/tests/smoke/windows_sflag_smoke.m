clear

% MATLAB R2025b: 6 windows accept 'periodic' / 'symmetric' (default).
%                6 others accept only typeName ('double' / 'single').

fprintf('=== periodic-supporting (sym vs per at index 2) ===\n');
ws = hamming(8);        wp = hamming(8, 'periodic');
fprintf('  hamming        sym=%.6f per=%.6f\n', ws(2), wp(2));
ws = hann(8);           wp = hann(8, 'periodic');
fprintf('  hann           sym=%.6f per=%.6f\n', ws(2), wp(2));
ws = blackman(8);       wp = blackman(8, 'periodic');
fprintf('  blackman       sym=%.6f per=%.6f\n', ws(2), wp(2));
ws = blackmanharris(8); wp = blackmanharris(8, 'periodic');
fprintf('  blackmanharris sym=%.6f per=%.6f\n', ws(2), wp(2));
ws = flattopwin(8);     wp = flattopwin(8, 'periodic');
fprintf('  flattopwin     sym=%.6f per=%.6f\n', ws(2), wp(2));
ws = nuttallwin(8);     wp = nuttallwin(8, 'periodic');
fprintf('  nuttallwin     sym=%.6f per=%.6f\n', ws(2), wp(2));

fprintf('\n=== typeName-only (must throw on periodic) ===\n');
fns = {'bartlett','triang','parzenwin','bohmanwin','barthannwin','rectwin'};
for k = 1:length(fns)
    fn = fns{k};
    try
        feval(fn, 8, 'periodic');
        fprintf('  %s: did NOT throw — BUG\n', fn);
    catch err
        fprintf('  %s: throws as expected (%s)\n', fn, err.message);
    end
end
