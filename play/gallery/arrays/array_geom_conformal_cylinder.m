% Конформный цилиндр
% Секторная коммутация на обтекателе

Nc = 32; Rc = 1.0; sector = 120;
th = -90:1:90;
AFc = zeros(1, numel(th));
for n = 1:Nc
    an = -180 + 360*(n-1)/Nc;
    if abs(an) <= sector/2
        AFc = AFc + exp(1i*2*pi*Rc*(cosd(an).*cosd(th) + sind(an).*sind(th)));
    end
end
figure;
polarplot(th*pi/180, abs(AFc)/max(1, sum(AFc ~= 0)));
title('Cylinder conformal, sector');

%{
@play
{
 "title": "Conformal cylinder",
 "groups": [
  {
   "name": "Geometry",
   "controls": [
    {
     "var": "Nc",
     "label": "Elements",
     "min": 8,
     "max": 64,
     "step": 4,
     "default": 32
    },
    {
     "var": "sector",
     "label": "Sector °",
     "min": 30,
     "max": 360,
     "step": 30,
     "default": 120
    }
   ]
  }
 ]
}
%}
