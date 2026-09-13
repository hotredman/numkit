% GMSK
% Гаусс-префильтр BT=0.2..0.5

bt = 0.3;
fs2 = 4000;
tp = -0.005:1/fs2:0.005;
imp = exp(-(tp/bt).^2/(2*log(2)));
figure;
plot(tp, imp./max(imp));
title('Gaussian prefilter (BT)'); xlabel('t'); grid on;

%{
@play
{
 "title": "GMSK",
 "groups": [
  {
   "name": "Фильтр",
   "controls": [
    {
     "var": "bt",
     "label": "BT",
     "min": 0.2,
     "max": 0.5,
     "step": 0.05,
     "default": 0.3
    }
   ]
  }
 ]
}
%}
