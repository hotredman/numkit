import { Component } from 'react';

/**
 * Catches render-time crashes inside the IDE so a localized bug doesn't
 * black out the entire window. Shows the error + stack and a "Reload" button.
 */
export default class ErrorBoundary extends Component {
  constructor(props) {
    super(props);
    this.state = { error: null, info: null };
  }
  static getDerivedStateFromError(error) {
    return { error, info: null };
  }
  componentDidCatch(error, info) {
    console.error('[IDE crash]', error, info);
    this.setState({ info });
  }
  render() {
    if (!this.state.error) return this.props.children;
    return (
      <div style={{
        position: 'fixed', inset: 0, padding: 24, overflow: 'auto',
        background: 'var(--bg-0)', color: 'var(--fg-0)',
        fontFamily: 'var(--font-mono)', fontSize: 12, lineHeight: 1.5,
        zIndex: 99999,
      }}>
        <div style={{ maxWidth: 900, margin: '0 auto' }}>
          <div style={{
            display: 'flex', alignItems: 'center', gap: 12, marginBottom: 16,
          }}>
            <span style={{
              padding: '3px 10px', borderRadius: 4,
              background: 'var(--danger)', color: '#fff',
              fontWeight: 600, fontSize: 11,
            }}>IDE crashed</span>
            <button onClick={() => location.reload()}
              style={{
                padding: '5px 12px', borderRadius: 4,
                background: 'var(--accent)', color: '#fff',
                border: 'none', cursor: 'pointer', fontSize: 11, fontWeight: 600,
              }}>Reload</button>
            <button onClick={() => this.setState({ error: null, info: null })}
              style={{
                padding: '5px 12px', borderRadius: 4,
                background: 'var(--bg-3)', color: 'var(--fg-1)',
                border: '1px solid var(--line)', cursor: 'pointer', fontSize: 11,
              }}>Try again</button>
          </div>
          <div style={{
            padding: 12, borderRadius: 6,
            background: 'var(--bg-1)', border: '1px solid var(--line)',
            marginBottom: 12,
          }}>
            <div style={{ color: 'var(--danger)', fontWeight: 600, marginBottom: 6 }}>
              {String(this.state.error?.name || 'Error')}: {String(this.state.error?.message || this.state.error)}
            </div>
            {this.state.error?.stack && (
              <pre style={{
                margin: 0, whiteSpace: 'pre-wrap', wordBreak: 'break-word',
                color: 'var(--fg-2)', fontSize: 11,
              }}>{this.state.error.stack}</pre>
            )}
          </div>
          {this.state.info?.componentStack && (
            <details>
              <summary style={{ cursor: 'pointer', color: 'var(--fg-2)', marginBottom: 8 }}>
                Component stack
              </summary>
              <pre style={{
                margin: 0, padding: 12, borderRadius: 6,
                background: 'var(--bg-1)', border: '1px solid var(--line)',
                color: 'var(--fg-2)', fontSize: 11, whiteSpace: 'pre-wrap',
              }}>{this.state.info.componentStack}</pre>
            </details>
          )}
        </div>
      </div>
    );
  }
}
