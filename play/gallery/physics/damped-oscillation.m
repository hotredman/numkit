% Damped oscillation: x(t) = exp(-zeta*w*t) * cos(wd*t)
zeta = 0.2;
f0 = 2;
w = 2 * pi * f0;
wd = w * sqrt(1 - zeta * zeta);
t = 0:0.005:3;
x = exp(-zeta * w * t) .* cos(wd * t);
env = exp(-zeta * w * t);
plot(t, x, 'b');
hold on;
plot(t, env, 'r--');
plot(t, -env, 'r--');
title('Damped oscillation');
xlabel('t, s');
ylabel('x(t)');
legend('x(t)', 'envelope');
grid on;

%{
@play
{
  "title": "Damped oscillation",
  "description": "Effect of the damping ratio on the transient response",
  "groups": [
    {
      "name": "System parameters",
      "controls": [
        { "var": "zeta", "label": "Damping ζ", "min": 0.05, "max": 1.0, "step": 0.05, "default": 0.2 },
        { "var": "f0", "label": "Frequency, Hz", "min": 0.5, "max": 5.0, "step": 0.5, "default": 2.0 }
      ]
    }
  ]
}
%}
