% Чебышёв I vs II
% Пульсации в ПП vs ПЗ

order = 4; fc = 0.2;
[b1c, a1c] = cheby1(order, 1, fc);
[b2c, a2c] = cheby2(order, 40, fc);
[H1, wf2] = freqz(b1c, a1c, 512);
[H2, ~] = freqz(b2c, a2c, 512);
figure;
plot(wf2/(2*pi), 20*log10(abs(H1)), wf2/(2*pi), 20*log10(abs(H2)));
legend('Cheby I','Cheby II');
title('Chebyshev compare'); grid on;

%{
@play
{
 "title": "Chebyshev I vs II",
 "groups": [
  {
   "name": "Filter",
   "controls": [
    {
     "var": "order",
     "label": "Order",
     "min": 2,
     "max": 8,
     "step": 2,
     "default": 4
    },
    {
     "var": "fc",
     "label": "Slice ×fs",
     "min": 0.1,
     "max": 0.4,
     "step": 0.05,
     "default": 0.2
    }
   ]
  }
 ]
}
%}
