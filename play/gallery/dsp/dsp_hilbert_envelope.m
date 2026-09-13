% Гильберт: огибающая
% Аналитический сигнал (степень 2 — workaround, см. hilbert bug)

decay = 3;
ts5 = 0:1/1000:0.063;
x5 = cos(2*pi*50*ts5).*exp(-decay*ts5);
z5 = hilbert(x5);
figure;
plot(ts5, x5, ts5, abs(z5));
legend('x(t)','|x(t)|');
title('Analytic signal envelope'); grid on;

%{
@play
{
 "title": "Hilbert: envelope",
 "groups": [
  {
   "name": "Signal",
   "controls": [
    {
     "var": "decay",
     "label": "Attenuation",
     "min": 1,
     "max": 8,
     "step": 1,
     "default": 3
    }
   ]
  }
 ]
}
%}
