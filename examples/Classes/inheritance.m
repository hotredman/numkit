% Inheritance — Shape -> Circle, Rectangle
% ========================================
% A subclass (`classdef Sub < Base`) inherits the base's properties and
% methods. It can:
%   * call the base constructor with `obj = obj@Base(args)`,
%   * override a method (here `area`), and
%   * inherit a method unchanged (here `describe`).
%
% Note `describe` is defined ONCE on Shape, yet it prints the right area
% for each subclass — calling `obj.area()` dispatches to the subclass's
% override (polymorphism).

clear

classdef Shape
  properties
    name = 'shape'
  end
  methods
    function obj = Shape(name)
      obj.name = name;
    end
    function a = area(obj)
      a = 0;                         % base default — subclasses override
    end
    function describe(obj)           % inherited as-is by every subclass
      fprintf('%-9s area = %8.4f\n', obj.name, obj.area());
    end
  end
end

classdef Circle < Shape
  properties
    radius = 1
  end
  methods
    function obj = Circle(r)
      obj = obj@Shape('circle');     % initialise the Shape part
      obj.radius = r;
    end
    function a = area(obj)
      a = pi * obj.radius^2;         % override
    end
  end
end

classdef Rectangle < Shape
  properties
    width  = 1
    height = 1
  end
  methods
    function obj = Rectangle(w, h)
      obj = obj@Shape('rectangle');
      obj.width  = w;
      obj.height = h;
    end
    function a = area(obj)
      a = obj.width * obj.height;    % override
    end
  end
end

% describe() is inherited; area() inside it dispatches to each override.
shapes = {Circle(2), Rectangle(3, 4)};
for i = 1:numel(shapes)
  shapes{i}.describe();
end

% isa() walks the hierarchy.
c = Circle(2);
fprintf('\nclass(c)          = %s\n', class(c));
fprintf('isa(c, ''Circle'')  = %d\n', isa(c, 'Circle'));
fprintf('isa(c, ''Shape'')   = %d   (ancestor)\n', isa(c, 'Shape'));
fprintf('isa(c, ''Rectangle'') = %d\n', isa(c, 'Rectangle'));
