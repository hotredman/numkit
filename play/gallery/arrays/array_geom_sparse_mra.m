% Разреженная MRA
% Эквивалентная апертура при минимуме приёмников

spacing = 1;
mra = [0 1 3 6 8 11 13 16] * spacing;
theta = -90:0.5:90;
AFm = zeros(1, numel(theta));
for n = 1:numel(mra)
    AFm = AFm + exp(1i*pi*mra(n)*sind(theta));
end
figure;
plot(theta, 20*log10(abs(AFm)/numel(mra) + 1e-12));
title('MRA sparse array'); ylabel('dB'); grid on;

%{
@play
{
 "title": "Разреженная MRA",
 "groups": [
  {
   "name": "Масштаб",
   "controls": [
    {
     "var": "spacing",
     "label": "Масштаб позиций",
     "min": 1,
     "max": 3,
     "step": 1,
     "default": 1
    }
   ]
  }
 ]
}
%}
