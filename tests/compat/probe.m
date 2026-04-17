function r = probe(a, b, c)
    % Helper for section C of matlab_parity_checks.m — calling this from
    % the base workspace triggers whos inside a function scope.
    whos
    r = a + b + c;
end
