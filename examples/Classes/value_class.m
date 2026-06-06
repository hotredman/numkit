% Value classes — Point2D
% =======================
% A *value* class (the default `classdef`, no `< handle`) behaves like a
% number or a struct: assigning it or passing it to a function makes an
% independent COPY. Mutating the copy never touches the original.
%
% This Point2D stores x/y, computes its norm, and returns a *new* scaled
% point from `scaled()` (the value-class idiom — methods that "change" the
% object return a fresh one). A custom `disp` controls how it prints.

clear

classdef Point2D
  properties
    x = 0
    y = 0
  end
  methods
    function obj = Point2D(x, y)
      obj.x = x;
      obj.y = y;
    end

    % sqrt(x^2 + y^2)
    function d = norm(obj)
      d = sqrt(obj.x^2 + obj.y^2);
    end

    % Returns a NEW point — the value-class way to "modify".
    function obj = scaled(obj, factor)
      obj.x = obj.x * factor;
      obj.y = obj.y * factor;
    end

    % Custom display — controls how `disp(p)` prints the object.
    function disp(obj)
      fprintf('Point2D(%g, %g)\n', obj.x, obj.y);
    end
  end
end

% ── Construct + call methods ──────────────────────────────────
p = Point2D(3, 4);
fprintf('p          = '); disp(p);
fprintf('p.norm()   = %g\n', p.norm());

% Method chaining: scaled() returns a Point2D, norm() runs on it.
fprintf('p.scaled(2).norm() = %g\n\n', p.scaled(2).norm());

% ── Value semantics: the copy is independent ──────────────────
q = p;              % copy
q = q.scaled(10);   % change the copy only
fprintf('q = q.scaled(10):\n');
fprintf('  q = '); disp(q);   % 30, 40
fprintf('  p = '); disp(p);   % still 3, 4 — original untouched

% ── Introspection ─────────────────────────────────────────────
fprintf('\nclass(p)        = %s\n', class(p));
fprintf('isobject(p)     = %d\n', isobject(p));
fprintf('num properties  = %d\n', numel(properties(p)));
fprintf('num methods     = %d\n', numel(methods(p)));
