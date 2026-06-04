import { useState, useMemo } from 'react';

/**
 * Default reference catalogue. Caller may override via the `docs` prop to plug
 * in a richer dataset built from the existing help.js / cheatsheet.js.
 *
 * Doc shape:
 *   { name, cat, sig, sum, syntax:[], desc, params:[[n,t,d]], returns:[[n,t,d]],
 *     examples:[], seeAlso:[] }
 */
export const REF_DOCS = [
  {
    name: 'filtfilt', cat: 'Signal',
    sig: 'y = filtfilt(b, a, x)', sum: 'Zero-phase digital filtering.',
    syntax: ['y = filtfilt(b, a, x)', 'y = filtfilt(sos, g, x)', 'y = filtfilt(b, a, x, padlen)'],
    desc: 'Performs zero-phase digital filtering by processing the input data x in both the forward and reverse directions. After filtering in the forward direction, filtfilt reverses the filtered sequence and runs it back through the filter. The result has zero phase distortion and a filter transfer function equal to the squared magnitude of the original.',
    params: [
      ['b, a',   'vector',           'Numerator and denominator coefficients of a rational transfer function.'],
      ['sos',    'matrix',           'Second-order sections matrix, K×6.'],
      ['g',      'vector',           'Scale-value vector of length K+1.'],
      ['x',      'vector | matrix',  'Input signal. If a matrix, each column is filtered independently.'],
      ['padlen', 'integer',          'Length of edge padding (default: 3·(max(len(a),len(b))-1)).'],
    ],
    returns: [['y', 'vector | matrix', 'Filtered signal, same size as x.']],
    examples: [
      `% Zero-phase low-pass\n[b, a] = butter(4, 0.2);\ny = filtfilt(b, a, x);`,
      `% SOS form for higher orders\n[z, p, k] = butter(8, [0.05 0.4]);\n[sos, g]    = zp2sos(z, p, k);\ny = filtfilt(sos, g, x);`,
    ],
    seeAlso: ['filter', 'butter', 'sosfilt', 'designfilt'],
  },
  {
    name: 'butter', cat: 'Signal',
    sig: '[b, a] = butter(n, Wn)', sum: 'Butterworth filter design.',
    syntax: [
      '[b, a]      = butter(n, Wn)',
      '[b, a]      = butter(n, Wn, ftype)',
      '[z, p, k]   = butter(n, Wn)',
      '[A, B, C, D]= butter(n, Wn)',
    ],
    desc: 'Designs an n-th-order lowpass digital Butterworth filter with normalized cutoff frequency Wn (where 1.0 corresponds to half the sample rate). Returns the filter coefficients in the (b, a) length n+1 row vectors with coefficients in descending powers of z.',
    params: [
      ['n',     'integer',                                  'Filter order.'],
      ['Wn',    'scalar | 2-vector',                        'Normalized cutoff in (0, 1). Two-element vector for bandpass/bandstop.'],
      ['ftype', "'low' | 'high' | 'bandpass' | 'stop'",     'Filter type. Default depends on length of Wn.'],
    ],
    returns: [
      ['b, a',    'vector',               'Transfer function coefficients.'],
      ['z, p, k', 'zeros / poles / gain', 'Pole-zero-gain form.'],
    ],
    examples: [
      `% 4th-order low-pass at 0.2·Fs/2\n[b, a] = butter(4, 0.2);`,
      `% Bandpass 50–200 Hz @ Fs=1 kHz\nFs = 1000;\n[b, a] = butter(6, [50 200]/(Fs/2), 'bandpass');`,
    ],
    seeAlso: ['filtfilt', 'cheby1', 'ellip', 'designfilt'],
  },
  {
    name: 'fft', cat: 'Transforms',
    sig: 'Y = fft(x)', sum: 'Discrete Fourier transform.',
    syntax: ['Y = fft(x)', 'Y = fft(x, n)', 'Y = fft(x, n, dim)'],
    desc: 'Computes the discrete Fourier transform (DFT) of x using a fast Fourier transform (FFT) algorithm. If x is a matrix, fft treats the columns of x as vectors and returns the Fourier transform of each column.',
    params: [
      ['x',   'vector | matrix', 'Input signal.'],
      ['n',   'integer',         'Transform length. x is padded with zeros or truncated.'],
      ['dim', 'integer',         'Dimension to operate along.'],
    ],
    returns: [['Y', 'complex array', 'Fourier coefficients, same size as zero-padded x.']],
    examples: [`Fs = 1000;  t = (0:1/Fs:1-1/Fs).';\nx  = sin(2*pi*50*t) + sin(2*pi*120*t);\nY  = fft(x);\nP  = abs(Y/length(x));`],
    seeAlso: ['ifft', 'fft2', 'fftshift', 'pwelch'],
  },
  {
    name: 'plot', cat: 'Plotting',
    sig: 'plot(x, y, ...)', sum: 'Linear 2-D plot.',
    syntax: ['plot(y)', 'plot(x, y)', 'plot(x, y, lineSpec)', 'plot(x1, y1, ..., xn, yn)', 'plot(ax, ___)'],
    desc: 'Creates a 2-D line plot of the data in y versus the corresponding values in x. To plot multiple data sets, supply multiple x, y pairs, each with an optional line spec string. Properties such as LineWidth, Color, Marker can be set with name-value pairs.',
    params: [
      ['x',        'vector | matrix', 'X-axis data.'],
      ['y',        'vector | matrix', 'Y-axis data.'],
      ['lineSpec', 'string',          "Line style/color/marker, e.g. '--r', 'o-'."],
      ['ax',       'axes handle',     'Target axes.'],
    ],
    returns: [['h', 'Line array', 'Array of Line objects, one per dataset.']],
    examples: [`t = 0:0.01:2*pi;\nplot(t, sin(t), '-', t, cos(t), '--');\nlegend('sin','cos');`],
    seeAlso: ['stem', 'semilogy', 'loglog', 'plot3'],
  },
  {
    name: 'zeros', cat: 'Arrays',
    sig: 'A = zeros(m, n)', sum: 'Matrix of zeros.',
    syntax: ['A = zeros(n)', 'A = zeros(m, n)', 'A = zeros(sz)', 'A = zeros(___, type)'],
    desc: 'Returns an m-by-n matrix of zeros. Default type is double; pass a typename to allocate other numeric types.',
    params: [
      ['m, n', 'integer', 'Output dimensions.'],
      ['sz',   'vector',  'Size vector for >2-D arrays.'],
      ['type', 'string',  "Typename, e.g. 'single', 'int32'."],
    ],
    returns: [['A', 'matrix', 'Zero-initialized array.']],
    examples: [`A = zeros(3, 4);\nB = zeros(8, 'single');`],
    seeAlso: ['ones', 'eye', 'nan', 'rand'],
  },
  {
    name: 'randn', cat: 'Arrays',
    sig: 'A = randn(m, n)', sum: 'Normally distributed pseudo-random numbers.',
    syntax: ['A = randn(n)', 'A = randn(m, n)', 'A = randn(sz)'],
    desc: 'Returns an array of standard-normal (μ=0, σ=1) pseudo-random numbers. Generator state can be controlled with rng().',
    params: [['m, n', 'integer', 'Output dimensions.'], ['sz', 'vector', 'Size vector.']],
    returns: [['A', 'matrix', 'Random sample.']],
    examples: [`rng(42);\nx = randn(1000, 1);  % 1000-sample noise vector`],
    seeAlso: ['rand', 'randi', 'rng'],
  },
  {
    name: 'mean', cat: 'Statistics',
    sig: 'm = mean(x)', sum: 'Average or mean value.',
    syntax: ['m = mean(x)', 'm = mean(x, dim)', "m = mean(___, 'omitnan')"],
    desc: 'Computes the arithmetic mean of x along the first non-singleton dimension by default. For matrices, mean(x) returns a row vector containing the mean of each column.',
    params: [['x', 'vector | matrix', 'Input data.'], ['dim', 'integer', 'Dimension to reduce.']],
    returns: [['m', 'scalar | vector', 'Computed mean.']],
    examples: [`m = mean(x);                % column means\nm2 = mean(M, 2);            % row means\nm3 = mean(x, 'omitnan');`],
    seeAlso: ['median', 'std', 'var', 'sum'],
  },
  {
    name: 'std', cat: 'Statistics',
    sig: 's = std(x)', sum: 'Standard deviation.',
    syntax: ['s = std(x)', 's = std(x, w)', 's = std(x, w, dim)'],
    desc: 'Returns the standard deviation of x. By default, uses the unbiased estimator (n-1 in the denominator). Pass w=1 to use the biased estimator (n).',
    params: [['x', 'vector | matrix', 'Input.'], ['w', '0 | 1', 'Weighting flag.'], ['dim', 'integer', 'Dimension to reduce.']],
    returns: [['s', 'scalar | vector', 'Standard deviation.']],
    examples: [`s  = std(x);\ns1 = std(x, 1);    % biased`],
    seeAlso: ['var', 'mean', 'mad', 'median'],
  },
  {
    name: 'pwelch', cat: 'Signal',
    sig: '[Pxx, f] = pwelch(x, win, nover, nfft, Fs)', sum: "Welch's power spectral density estimate.",
    syntax: ['[Pxx, f] = pwelch(x)', '[Pxx, f] = pwelch(x, win)', '[Pxx, f] = pwelch(x, win, nover, nfft, Fs)'],
    desc: "Estimates the power spectral density (PSD) of the input signal using Welch's overlapped segment averaging method. The signal is divided into overlapping segments, each windowed and FFT'd; the periodograms are averaged.",
    params: [
      ['x',     'vector',            'Input signal.'],
      ['win',   'integer | vector',  'Window length or window vector.'],
      ['nover', 'integer',           'Number of overlapping samples (default 50%).'],
      ['nfft',  'integer',           'FFT length.'],
      ['Fs',    'scalar',            'Sample rate (Hz).'],
    ],
    returns: [['Pxx', 'vector', 'PSD estimate.'], ['f', 'vector', 'Frequency vector (Hz).']],
    examples: [`[Pxx, f] = pwelch(x, hann(1024), 512, 2048, Fs);\nplot(f, 10*log10(Pxx));`],
    seeAlso: ['fft', 'periodogram', 'spectrogram'],
  },
  {
    name: 'linspace', cat: 'Arrays',
    sig: 'y = linspace(a, b, n)', sum: 'Linearly spaced vector.',
    syntax: ['y = linspace(a, b)', 'y = linspace(a, b, n)'],
    desc: 'Generates n evenly spaced points between a and b, inclusive. Default n is 100.',
    params: [['a, b', 'scalar', 'Start and end values.'], ['n', 'integer', 'Number of points.']],
    returns: [['y', 'row vector', 'Generated points.']],
    examples: [`t = linspace(0, 1, 1001);   % 1 ms grid over [0, 1]`],
    seeAlso: ['logspace', 'colon'],
  },
  {
    name: 'reshape', cat: 'Arrays',
    sig: 'B = reshape(A, sz)', sum: 'Reshape array.',
    syntax: ['B = reshape(A, sz)', 'B = reshape(A, m, n, ...)'],
    desc: 'Reshapes A into the size specified by sz, using its column-major order. The number of elements must match.',
    params: [['A', 'array', 'Input array.'], ['sz', 'vector', 'New size.']],
    returns: [['B', 'array', 'Reshaped array.']],
    examples: [`v = 1:12;\nM = reshape(v, [3, 4]);   % 3x4 matrix`],
    seeAlso: ['squeeze', 'permute', 'size'],
  },
  {
    name: 'find', cat: 'Arrays',
    sig: 'idx = find(x)', sum: 'Find indices of nonzero elements.',
    syntax: ['idx = find(x)', 'idx = find(x, k)', '[r, c] = find(x)'],
    desc: 'Returns a vector containing the linear indices of nonzero elements in x. With two outputs, returns row and column subscripts.',
    params: [['x', 'array', 'Input array (typically logical).'], ['k', 'integer', 'Return only first k indices.']],
    returns: [['idx', 'vector', 'Linear indices.']],
    examples: [`idx = find(x > 0.5);\n[r, c] = find(M ~= 0);`],
    seeAlso: ['any', 'all', 'nnz'],
  },
];

function ReferenceCard({ doc, onOpen }) {
  return (
    <button className="ref-card" onClick={onOpen}>
      <div className="ref-card-head">
        <span className="ref-card-name">{doc.name}</span>
        <span className="ref-card-cat">{doc.cat}</span>
      </div>
      <div className="ref-card-sig">{doc.sig}</div>
      <div className="ref-card-sum">{doc.sum}</div>
    </button>
  );
}

function ReferenceDetail({ doc, docs, onClose, onOpenName }) {
  return (
    <div className="ref-detail">
      <div className="ref-detail-bar">
        <button className="ref-back" onClick={onClose} aria-label="Back">
          <svg width="11" height="11" viewBox="0 0 12 12">
            <path d="M7 2L3 6L7 10" stroke="currentColor" strokeWidth="1.4" fill="none" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
          <span>Back</span>
        </button>
        <div className="ref-crumb">
          <span className="ref-crumb-cat">{doc.cat}</span>
          <span className="ref-crumb-sep">/</span>
          <span className="ref-crumb-name">{doc.name}</span>
        </div>
        <div className="ref-detail-spacer" />
        <button className="ref-detail-action" title="Copy signature"
          onClick={() => navigator.clipboard?.writeText(doc.sig)}>
          <svg width="11" height="11" viewBox="0 0 12 12">
            <rect x="3" y="3" width="7" height="7" rx="1" stroke="currentColor" fill="none"/>
            <path d="M2 8V2h6" stroke="currentColor" fill="none"/>
          </svg>
          Copy
        </button>
      </div>

      <div className="ref-detail-body">
        <div className="ref-block">
          <h2 className="ref-h1">{doc.name}</h2>
          <div className="ref-tagline">{doc.sum}</div>
        </div>

        <section className="ref-block">
          <h3 className="ref-h2">Syntax</h3>
          <pre className="ref-pre">{doc.syntax.join('\n')}</pre>
        </section>

        <section className="ref-block">
          <h3 className="ref-h2">Description</h3>
          <p className="ref-p">{doc.desc}</p>
        </section>

        {doc.params?.length > 0 && (
          <section className="ref-block">
            <h3 className="ref-h2">Input arguments</h3>
            <table className="ref-table">
              <tbody>
                {doc.params.map(([n, t, d], i) => (
                  <tr key={i}>
                    <td className="ref-table-name">{n}</td>
                    <td className="ref-table-type">{t}</td>
                    <td className="ref-table-desc">{d}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </section>
        )}

        {doc.returns?.length > 0 && (
          <section className="ref-block">
            <h3 className="ref-h2">Output arguments</h3>
            <table className="ref-table">
              <tbody>
                {doc.returns.map(([n, t, d], i) => (
                  <tr key={i}>
                    <td className="ref-table-name">{n}</td>
                    <td className="ref-table-type">{t}</td>
                    <td className="ref-table-desc">{d}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </section>
        )}

        {doc.examples?.length > 0 && (
          <section className="ref-block">
            <h3 className="ref-h2">Examples</h3>
            {doc.examples.map((ex, i) => (
              <pre key={i} className="ref-pre ref-pre-code">{ex}</pre>
            ))}
          </section>
        )}

        {doc.seeAlso?.length > 0 && (
          <section className="ref-block">
            <h3 className="ref-h2">See also</h3>
            <div className="ref-seealso">
              {doc.seeAlso.map((n) => {
                const found = docs.find((d) => d.name === n);
                return (
                  <button key={n} className={`ref-link ${found ? '' : 'is-extern'}`}
                    onClick={() => found && onOpenName(n)}>
                    {n}
                  </button>
                );
              })}
            </div>
          </section>
        )}
      </div>
    </div>
  );
}

export default function ReferencePanel({ docs = REF_DOCS }) {
  const [query, setQuery] = useState('');
  const [openName, setOpenName] = useState(null);

  const filtered = useMemo(() => {
    const q = query.trim().toLowerCase();
    if (!q) return docs;
    return docs.filter((d) =>
         d.name.toLowerCase().includes(q)
      || d.sum.toLowerCase().includes(q)
      || d.sig.toLowerCase().includes(q)
      || d.cat.toLowerCase().includes(q)
      || d.desc.toLowerCase().includes(q)
    );
  }, [query, docs]);

  const openDoc = openName ? docs.find((d) => d.name === openName) : null;

  if (openDoc) {
    return <ReferenceDetail doc={openDoc} docs={docs}
      onClose={() => setOpenName(null)} onOpenName={setOpenName} />;
  }

  return (
    <div className="reference">
      <div className="ref-toolbar">
        <span className="ref-count">{filtered.length} fn</span>
        <div className="ws-search">
          <svg width="11" height="11" viewBox="0 0 12 12" aria-hidden="true">
            <circle cx="5" cy="5" r="3.2" stroke="currentColor" strokeWidth="1.2" fill="none" />
            <path d="M7.4 7.4L10 10" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round" />
          </svg>
          <input value={query} onChange={(e) => setQuery(e.target.value)}
            placeholder="search functions, signatures, descriptions…" spellCheck={false} />
        </div>
      </div>

      <div className="ref-grid">
        {filtered.map((doc) => (
          <ReferenceCard key={doc.name} doc={doc} onOpen={() => setOpenName(doc.name)} />
        ))}
        {filtered.length === 0 && (
          <div className="ref-empty">No functions match “{query}”.</div>
        )}
      </div>
    </div>
  );
}
