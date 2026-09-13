% Сферическая решётка
% Изотропный обзор полусферы

Nrings = 5;
th = 0:2:180;
AFs = zeros(1, numel(th));
for ring = 0:Nrings-1
    elr = ring * 30;
    cnt = max(4, round(12*cosd(elr)));
    for m = 1:cnt
        az = 360*(m-1)/cnt;
        for it = 1:numel(th)
            u = cosd(th(it))*cosd(az) + sind(th(it))*sind(az);
            AFs(it) = AFs(it) + exp(1i*pi*u*sind(elr));
        end
    end
end
figure;
plot(th, abs(AFs)/max(abs(AFs)));
title('Spherical cap coverage');

%{
@play
{
 "title": "Сферическая решётка",
 "groups": [
  {
   "name": "Геометрия",
   "controls": [
    {
     "var": "Nrings",
     "label": "Число колец",
     "min": 2,
     "max": 8,
     "step": 1,
     "default": 5
    }
   ]
  }
 ]
}
%}
