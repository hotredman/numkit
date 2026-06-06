% Static methods & Constant properties — Physics
% ==============================================
% `properties (Constant)` are shared, read-only class data — reach them
% on the class name without ever making an instance: `Physics.C`.
% `methods (Static)` are plain functions namespaced under the class, with
% no `obj` receiver: `Physics.massEnergy(1)`. Together they make a tidy
% "utility class" — constants plus the formulas that use them.

clear

classdef Physics
  properties (Constant)
    C = 299792458        % speed of light in vacuum, m/s
    G = 6.674e-11        % gravitational constant, N*m^2/kg^2
  end
  methods (Static)
    function E = massEnergy(m)
      E = m * Physics.C^2;                 % E = m c^2
    end
    function F = gravity(m1, m2, r)
      F = Physics.G * m1 * m2 / r^2;       % Newton's law of gravitation
    end
  end
end

% Constants — no instance needed.
fprintf('speed of light c = %g m/s\n',  Physics.C);
fprintf('grav constant  G = %g\n\n',    Physics.G);

% Static methods — called on the class, combine freely with the constants.
fprintf('E = m c^2 for 1 kg      : %g J\n', Physics.massEnergy(1));
fprintf('Earth <-> Moon gravity  : %.3e N\n', ...
        Physics.gravity(5.972e24, 7.342e22, 3.844e8));
