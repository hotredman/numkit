/**
 * FigureErrorBoundary — local boundary that isolates one figure
 * renderer crash from the rest of the IDE. Unlike the global
 * ErrorBoundary in ../ErrorBoundary.jsx (which takes over the
 * whole window), this one only replaces the failing figure with
 * a small inline error tile so the figures pane / modal / subplot
 * grid stay alive and the user can keep working.
 *
 * Mounted around Composite3DPlot during the WebGL roll-out so a
 * mesh-builder bug doesn't black out the IDE.
 */
import { Component } from 'react';

export default class FigureErrorBoundary extends Component {
  constructor(props) {
    super(props);
    this.state = { error: null };
  }

  static getDerivedStateFromError(error) {
    return { error };
  }

  componentDidCatch(error, info) {
    const tag = this.props.label || 'figure';
    // eslint-disable-next-line no-console
    console.error(`[FigureBoundary:${tag}]`, error, info);
  }

  // Reset on figure identity change so a fresh figure isn't blocked
  // by a stale error from the previous one.
  componentDidUpdate(prevProps) {
    if (this.state.error && prevProps.figureId !== this.props.figureId) {
      this.setState({ error: null });
    }
  }

  reset = () => this.setState({ error: null });

  render() {
    if (this.state.error) {
      const w = this.props.width || 320;
      const h = this.props.height || 240;
      const msg = String(this.state.error.message || this.state.error);
      return (
        <div style={{
          width: w, height: h,
          display: 'flex', flexDirection: 'column',
          alignItems: 'flex-start', justifyContent: 'flex-start',
          padding: 12, gap: 6,
          background: 'var(--plot-bg, #0d1117)',
          color: 'var(--plot-text, #d4d4f0)',
          border: '1px solid var(--plot-frame, #444c56)',
          borderRadius: 4,
          fontFamily: 'monospace', fontSize: 11,
          overflow: 'hidden',
        }}>
          <div style={{ color: '#e26a6a', fontWeight: 600 }}>
            figure render error
          </div>
          <div style={{ opacity: 0.85,
            wordBreak: 'break-word', whiteSpace: 'pre-wrap',
            maxHeight: h - 70, overflow: 'auto',
          }}>{msg}</div>
          <button onClick={this.reset}
                  style={{
                    marginTop: 'auto',
                    padding: '4px 10px', fontSize: 11,
                    background: 'var(--bg-3, #2d333b)',
                    color: 'var(--fg-1, #d0d4dc)',
                    border: '1px solid var(--line, #444c56)',
                    cursor: 'pointer', borderRadius: 3,
                  }}>retry</button>
        </div>
      );
    }
    return this.props.children;
  }
}
