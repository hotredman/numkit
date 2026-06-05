% Operator overloading — Vector2
% ==============================
% Define methods with the special names MATLAB maps to operators and the
% operators "just work" on your objects:
%   plus   +      minus  -      uminus  (unary -)
%   mtimes *      eq     ==
% Each operator method takes the operands in source order and returns the
% result (here a new Vector2, or a logical for ==).

clear

classdef Vector2
  properties
    x = 0
    y = 0
  end
  methods
    function obj = Vector2(x, y)
      obj.x = x;
      obj.y = y;
    end

    function r = plus(a, b)          % a + b
      r = Vector2(a.x + b.x, a.y + b.y);
    end
    function r = minus(a, b)         % a - b
      r = Vector2(a.x - b.x, a.y - b.y);
    end
    function r = uminus(a)           % -a
      r = Vector2(-a.x, -a.y);
    end
    function r = mtimes(a, s)        % a * scalar
      r = Vector2(a.x * s, a.y * s);
    end
    function t = eq(a, b)            % a == b
      t = (a.x == b.x) && (a.y == b.y);
    end

    function d = norm(obj)
      d = sqrt(obj.x^2 + obj.y^2);
    end
    function disp(obj)
      fprintf('(%g, %g)\n', obj.x, obj.y);
    end
  end
end

u = Vector2(1, 2);
v = Vector2(3, 4);
fprintf('u        = '); disp(u);
fprintf('v        = '); disp(v);
fprintf('u + v    = '); disp(u + v);
fprintf('v - u    = '); disp(v - u);
fprintf('-u       = '); disp(-u);
fprintf('v * 2    = '); disp(v * 2);
fprintf('norm(v)  = %g\n', v.norm());
fprintf('u == u   : %d\n', u == u);
fprintf('u == v   : %d\n', u == v);

% Operators are left-associative, so this chains plus(plus(u,v),u).
fprintf('u + v + u = '); disp(u + v + u);
