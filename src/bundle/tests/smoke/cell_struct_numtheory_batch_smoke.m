clear

% Cell+struct + number-theory batch — spec closure 2026-05-09. 18 funcs.

c = cell(2,3); fprintf('cell(2,3) numel=%d, iscell=%d\n', numel(c), iscell(c));
fprintf('cellfun @(x)x*2 on {1,2,3}: '); disp(cellfun(@(x) x*2, {1,2,3}));
fprintf('cell2mat({1,2;3,4}):\n');       disp(cell2mat({1,2;3,4}));
s = struct('a', 1, 'b', 2);
fprintf('struct.a = %g, .b = %g\n', s.a, s.b);
fprintf('fieldnames: '); disp(fieldnames(s));
fprintf('isfield s.a = %d\n', isfield(s, 'a'));
fprintf('gcd(12,8) = %d, lcm(4,6) = %d\n', gcd(12,8), lcm(4,6));
fprintf('factorial(5) = %d\n', factorial(5));
fprintf('factor(60) = '); disp(factor(60));
fprintf('isprime(7) = %d, isprime(8) = %d\n', isprime(7), isprime(8));
fprintf('primes(20) = '); disp(primes(20));
fprintf('nchoosek(5,2) = %d\n', nchoosek(5,2));
fprintf('size(perms(1:3)) = '); disp(size(perms(1:3)));
