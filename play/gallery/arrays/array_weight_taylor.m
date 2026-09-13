% Распределение Тейлора
% Спадающие боковые — стандарт радаров

N = 16;
sll = -30;
w = taylorwin(N, 4, sll);
theta = -90:0.2:90;
AF = zeros(1, numel(theta));
for k = 1:N
    AF = AF + w(k)*exp(1i*pi*(k-1)*sind(theta));
end
figure;
plot(theta, 20*log10(abs(AF)/sum(w) + 1e-12));
title('Taylor taper'); ylabel('dB'); grid on;

%{
@play
{
 "title": "Распределение Тейлора",
 "groups": [
  {
   "name": "Синтез",
   "controls": [
    {
     "var": "N",
     "label": "N",
     "min": 8,
     "max": 32,
     "step": 2,
     "default": 16
    },
    {
     "var": "sll",
     "label": "SLL дБ",
     "min": -40,
     "max": -15,
     "step": 5,
     "default": -30
    }
   ]
  }
 ]
}
%}
