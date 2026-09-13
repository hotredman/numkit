% Cardioid element
% F = |1 + beta*cos(theta)| — rear-lobe suppression

beta = 10;
th = linspace(0, 2*pi, 361);
pat = abs(1 + beta/10*cos(th));    % normalized element pattern (peak 1)
figure;
polarplot(th, pat);
title('Cardioid element');

%{
@play
{
 "title": "Cardioid element",
 "groups": [
  {
   "name": "Shape",
   "controls": [
    {
     "var": "beta",
     "label": "β ×10",
     "min": 0,
     "max": 10,
     "step": 1,
     "default": 10
    }
   ]
  }
 ]
}
%}
