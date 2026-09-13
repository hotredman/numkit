% SOFAR канал
% Волновод вокруг минимума c(z)

depth = 200; nrays = 7;
zr2 = linspace(0, 400, 200);
csm = 1490 - 20*exp(-((zr2-depth)/80).^2);
figure;
plot(csm, zr2);
title('SOFAR profile'); xlabel('c м/с'); ylabel('z м'); grid on;

%{
@play
{
 "title": "SOFAR channel",
 "groups": [
  {
   "name": "Profile",
   "controls": [
    {
     "var": "depth",
     "label": "Channel axis m",
     "min": 100,
     "max": 400,
     "step": 50,
     "default": 200
    },
    {
     "var": "nrays",
     "label": "Ray count",
     "min": 3,
     "max": 11,
     "step": 2,
     "default": 7
    }
   ]
  }
 ]
}
%}
