% MSK
% Непрерывная фаза, постоянная огибающая

bitRate = 10;
fs2 = 4000;
tt = 0:1/fs2:0.02;
data = repelem(randi([0 1], 1, 10), max(1, ceil(numel(tt)/10)));
data = data(1:numel(tt));
phM = 2*pi*cumsum(data - 0.5)/(2*fs2)*1000;
figure;
plot(tt(1:numel(phM)), cos(phM));
title('MSK constant envelope'); grid on;

%{
@play
{
 "title": "MSK",
 "groups": [
  {
   "name": "Сигнал",
   "controls": [
    {
     "var": "bitRate",
     "label": "Бит/с ×10³",
     "min": 1,
     "max": 20,
     "step": 1,
     "default": 10
    }
   ]
  }
 ]
}
%}
