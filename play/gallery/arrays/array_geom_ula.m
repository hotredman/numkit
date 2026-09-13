% ULA: grating-lobe onset vs d/lambda
% Linear equidistant array: beam distortion at d/lambda > 1
N = 16;
d10 = 5;                        % d/lambda * 10
theta0 = 20;                    % steer angle, deg
d = d10 / 10;
theta = -90:0.2:90;
ph = 2*pi*d*(sind(theta) - sind(theta0));
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + exp(1i*(k-1)*ph);
end
pat = abs(AF) / N;              % normalized beam pattern (peak 1)
figure;
plot(theta, pat);
title('ULA beam pattern');
xlabel('\theta deg'); ylabel('|AF|/N');
grid on;

%{
@play
{
 "title": "ULA: element spacing and grating lobes",
 "groups": [
  { "name": "Geometry", "controls": [
    { "var": "N", "label": "Element count", "min": 8, "max": 64, "step": 2, "default": 16 },
    { "var": "d10", "label": "d/λ ×10", "min": 2, "max": 15, "step": 1, "default": 5 },
    { "var": "theta0", "label": "Scan θ₀", "min": -60, "max": 60, "step": 1, "default": 20 }
  ] }
 ]
}
%}
