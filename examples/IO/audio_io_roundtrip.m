% Audio I/O Round-trip — audiowrite, audioinfo, audioread
% Synthesize a harmonic musical chord with exponential decay,
% save to a 16-bit PCM WAV audio file, read back audio data & metadata,
% and plot the waveform along with its time-frequency spectrogram.

clear
close all

% 1. Synthesize a 1-second audio chord (A-major: A4 440 Hz, C#5 554 Hz, E5 659 Hz)
fs = 8000;
t  = (0:1/fs:1-1/fs)';

env = exp(-3 * t);  % exponential amplitude envelope
y   = 0.3 * env .* (sin(2*pi*440*t) + sin(2*pi*554.37*t) + sin(2*pi*659.25*t));

% 2. Save audio to WAV file
audio_file = 'demo_chord.wav';
audiowrite(audio_file, y, fs, 'BitsPerSample', 16);

% 3. Inspect audio metadata via audioinfo
info = audioinfo(audio_file);
fprintf('Sample Rate:   %d Hz\n',   info.SampleRate);
fprintf('Channels:      %d\n',      info.NumChannels);
fprintf('Total Samples: %d\n',      info.TotalSamples);
fprintf('Duration:      %.2f s\n',  info.Duration);

% 4. Read back the audio signal
[y_loaded, fs_loaded] = audioread(audio_file);

% 5. Plot the audio waveform and its spectrogram
figure;
subplot(2, 1, 1);
plot(t, y_loaded);
xlabel('Time (s)');
ylabel('Amplitude');
title('Decaying A-Major Chord Waveform (WAV)');
grid on;

subplot(2, 1, 2);
spectrogram(y_loaded, 256, 200, 512, fs_loaded, 'yaxis');
title('Audio Spectrogram');
sgtitle('Audio File I/O (audiowrite / audioread / audioinfo)');
