clear;

% figure_export — saveas / exportgraphics / print (portion 32).

x = 0:0.05:6.28;
plot(x, sin(x), 'b-', x, cos(x), 'r--o');
title('Two waves');
xlabel('time [s]');
ylabel('amplitude');
legend('sin', 'cos');
grid on;

d = tempdir;
saveas(gcf, [d 'smoke_export.png']);
fprintf('png exist = %g (expect 2)\n', exist([d 'smoke_export.png'], 'file'));

saveas(gcf, [d 'smoke_export.svg'], 'svg');
fprintf('svg exist = %g (expect 2)\n', exist([d 'smoke_export.svg'], 'file'));

print(gcf, '-dpng', [d 'smoke_print.png']);
fprintf('print png exist = %g (expect 2)\n', exist([d 'smoke_print.png'], 'file'));

subplot(2,1,1); plot(1:5); title('Upper');
subplot(2,1,2); bar([3 1 4]); title('Lower');
exportgraphics(gcf, [d 'smoke_subplot.png'], 'Resolution', 200);
fprintf('subplot png exist = %g (expect 2)\n', exist([d 'smoke_subplot.png'], 'file'));

try
  saveas(gcf, [d 'smoke_export.pdf']);
  disp('pdf accepted - BAD');
catch e
  disp(['pdf rejected: ' e.message]);
end
