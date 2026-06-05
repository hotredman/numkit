% Dependent properties — Temperature
% ==================================
% A `properties (Dependent)` value isn't stored — it's COMPUTED on access
% by a `get.<name>` method, and writing it runs a `set.<name>` method that
% updates the real stored state. Here only `celsius` is stored; reading or
% writing `fahrenheit` converts on the fly, so the two can never disagree.

clear

classdef Temperature
  properties
    celsius = 0                      % the one stored value
  end
  properties (Dependent)
    fahrenheit                       % computed from celsius
  end
  methods
    function obj = Temperature(c)
      obj.celsius = c;
    end

    % getter — runs whenever fahrenheit is READ.
    function f = get.fahrenheit(obj)
      f = obj.celsius * 9/5 + 32;
    end

    % setter — runs whenever fahrenheit is WRITTEN; updates celsius.
    function obj = set.fahrenheit(obj, f)
      obj.celsius = (f - 32) * 5/9;
    end
  end
end

t = Temperature(100);
fprintf('Temperature(100):\n');
fprintf('  celsius    = %g\n',  t.celsius);
fprintf('  fahrenheit = %g   (computed by get.fahrenheit)\n\n', t.fahrenheit);

% Writing the dependent property runs set.fahrenheit, which writes celsius.
t.fahrenheit = 32;
fprintf('after  t.fahrenheit = 32:\n');
fprintf('  celsius    = %g    (set.fahrenheit wrote through)\n', t.celsius);
fprintf('  fahrenheit = %g\n', t.fahrenheit);
