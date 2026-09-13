% 16-QAM + фазовый шум
% Круговое размытие созвездия

phn = 0.15;
points = sqrt(2)*((0:3)-1.5);
[Ix, Iy] = meshgrid(points, points);
Iq = Ix(:) + 1i*Iy(:);
rx = Iq .* exp(1i*randn(1, numel(Iq))*phn) + randn(1, numel(Iq))*0.05;
figure;
plot(real(rx), imag(rx), '.');
title('16-QAM, phase noise'); axis equal; grid on;

%{
@play
{
 "title": "16-QAM + phase noise",
 "groups": [
  {
   "name": "Heterodyne",
   "controls": [
    {
     "var": "phn",
     "label": "phase noise σ",
     "min": 0.02,
     "max": 0.4,
     "step": 0.02,
     "default": 0.15
    }
   ]
  }
 ]
}
%}
