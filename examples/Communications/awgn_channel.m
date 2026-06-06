% awgn_channel.m
% Show that awgn() actually delivers the requested SNR by passing
% a sinusoid through and measuring SNR on the output.

import compat.*;

clear
close all

fs = 1e3; t = (0:1/fs:1-1/fs)';
x  = sin(2*pi*50*t);
Px = mean(x.^2);

SNRs_req = [30, 20, 10, 0, -5];
for k = 1:numel(SNRs_req)
    y  = awgn(x, SNRs_req(k), 'measured');
    Pn = mean((y - x).^2);
    SNR_meas = 10*log10(Px / Pn);
    fprintf('  Requested SNR = %5.1f dB,  measured = %5.2f dB\n', ...
            SNRs_req(k), SNR_meas);
end

% Visualise the time-domain effect at three SNRs.
figure;
for k = 1:3
    snr = [20 10 0];
    y = awgn(x, snr(k), 'measured');
    subplot(3, 1, k);
    plot(t(1:200), y(1:200));
    title(sprintf('Sinusoid + AWGN at SNR = %d dB', snr(k)));
    grid on;
end
xlabel('time (s)');
sgtitle('AWGN channel — output at three SNR levels');
