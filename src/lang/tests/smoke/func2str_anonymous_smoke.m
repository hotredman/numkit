clear
import compat.*

% func2str on anonymous handles (bugs/runtime/func2str-anonymous) — used to
% return the internal '@__anon_N' placeholder; now reconstructs the source with
% MATLAB-normalized whitespace (inter-token spaces dropped, literals verbatim).

fprintf('%-24s (expect @(x)x+1)\n',          func2str(@(x) x + 1));
fprintf('%-24s (expect @(a,b)a.*b+1)\n',     func2str(@(a,b) a.*b + 1));
fprintf('%-24s (expect @()42)\n',            func2str(@() 42));
fprintf('%-24s (expect @(s)[s,'' world''])\n', func2str(@(s) [s, ' world']));
fprintf('%-24s (expect sin  -- named handle)\n', func2str(@sin));

% str2func(func2str(h)) round-trips for anonymous handles
h  = @(x) x.^2 - 3;
h2 = str2func(func2str(h));
fprintf('round-trip h2(4) = %g (expect 13)\n', h2(4));
