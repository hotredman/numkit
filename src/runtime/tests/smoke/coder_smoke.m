clear
import compat.*
% coder / coder_run / system / runNative smoke — AOT codegen + process builtins.
% Run: build/desktop-fast/apps/numkit/Release/numkit.exe \
%      src/runtime/tests/smoke/coder_smoke.m
% Reference: numkit codegen DESIGN.md §8/§9. coder_run needs a C++ compiler
% (NUMKIT_CXX or the build-time default); runNative needs NUMKIT_INTERP.

% --- coder: transpile a nullary scalar function to C++ ---
src = 'function y = f(); y = 3.14; end';
cpp = coder(src, 'f', '');
fprintf('coder: %d chars (e ~5000+), starts: %s\n', numel(cpp), cpp(1:min(40,numel(cpp))));
fprintf('  has "double f_" : %d (e 1)\n', contains(cpp, 'double f_'));
fprintf('  has "return y"  : %d (e 1)\n', contains(cpp, 'return y'));

% --- coder_run: transpile + compile + run (needs a C++ compiler) ---
try
  out = coder_run(src, 'f', '');
  fprintf('coder_run: out=%s (e 3.1400000000000001)\n', strtrim(out));
catch e
  fprintf('coder_run: skipped (%s)\n', e.message);
end

% --- system: echo via the shell, two-output form ---
[s, o] = system('echo nk_system_smoke');
fprintf('system: status=%d (e 0), out=%s (e nk_system_smoke)\n', s, strtrim(o));

% --- runNative: run a .m via NUMKIT_INTERP (set it first) ---
try
  [sn, on] = runNative('src\math\tests\smoke\sinpi_smoke.m');
  fprintf('runNative: status=%d (e 0), first line: %s\n', sn, strtrim(on(1:min(40,numel(on)))));
catch e
  fprintf('runNative: skipped (%s)\n', e.message);
end
