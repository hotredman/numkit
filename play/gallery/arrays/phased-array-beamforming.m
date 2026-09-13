% =========================================================================
% Phased array: array factor in polar coordinates
% =========================================================================
N = 16;
theta0 = 25;
d = 0.5;
win = 'hamming';
% Weighting coefficients
if strcmp(win, 'chebwin')
    w = chebwin(N, 30);
elseif strcmp(win, 'hamming')
    w = hamming(N);
else
    w = ones(N, 1);
end
% Angular sweep and the array factor
theta = -90:0.5:90;
ph = 2 * pi * d * (sin(theta * pi / 180) - sin(theta0 * pi / 180));
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + w(k) * exp(1i * (k - 1) * ph);
end
mag = abs(AF) / sum(w);            % normalized beam pattern (peak 1)
figure;
polarplot(theta * pi / 180, mag);
title('Phased array beam pattern');

%{
@play
{
  "title": "Phased array (beamforming)",
  "description": "Interactive beam pattern and sidelobe suppression",
  "groups": [
    {
      "name": "Antenna geometry",
      "controls": [
        { "var": "N", "label": "Element count", "min": 8, "max": 64, "step": 2, "default": 16 },
        { "var": "d", "label": "Element spacing (d/λ)", "min": 0.1, "max": 1.5, "step": 0.05, "default": 0.5 }
      ]
    },
    {
      "name": "Beam steering",
      "controls": [
        { "var": "theta0", "label": "Rotation angle", "min": -60, "max": 60, "step": 1, "unit": "°", "default": 25 },
        { "var": "win", "label": "Window function", "type": "select", "options": ["rect", "hamming", "chebwin"], "default": "hamming" }
      ]
    }
  ]
}
%}
