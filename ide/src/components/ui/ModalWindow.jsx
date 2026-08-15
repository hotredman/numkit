import React, { useState, useEffect, useCallback } from 'react';

/**
 * ModalWindow — Unified, accessible modal dialog base for Numkit IDE widgets.
 *
 * Features:
 * - Backdrop overlay with blur & smooth fade-in
 * - Consistent border, rounded corners, and elevation glow shadow
 * - Fullscreen maximize / restore support with toggle SVG icon
 * - Standard titlebar with tag badge, title, subtitle, meta, and pinned controls
 * - Escape key dismissal & backdrop click-to-close
 * - Optional footer slot
 *
 * @param {object} props
 * @param {() => void} props.onClose - Called to dismiss the modal
 * @param {string|React.ReactNode} props.title - Main dialog title
 * @param {string|{label: string, color?: string, bg?: string, border?: string}|React.ReactNode} [props.tag] - Tag badge
 * @param {string|React.ReactNode} [props.subtitle] - Secondary title or subtitle
 * @param {string|React.ReactNode} [props.meta] - Additional metadata text in titlebar
 * @param {React.ReactNode} [props.customTitleLeft] - Full override for title-left slot
 * @param {React.ReactNode} [props.customTitleRight] - Extra controls before maximize/close
 * @param {boolean} [props.allowMaximize=true] - Whether to show maximize/restore button
 * @param {boolean} [props.maximized] - Controlled maximized state
 * @param {(max: boolean) => void} [props.onMaximizedChange] - Controlled maximize handler
 * @param {boolean} [props.defaultMaximized=false] - Initial maximize state if uncontrolled
 * @param {string|number} [props.width='min(1150px, 92vw)'] - Modal default width
 * @param {string|number} [props.height='min(760px, 88vh)'] - Modal default height
 * @param {string|number} [props.maxWidth] - Max width
 * @param {string|number} [props.maxHeight] - Max height
 * @param {string} [props.className=''] - Extra classes for modal window
 * @param {string} [props.overlayClassName=''] - Extra classes for overlay
 * @param {string} [props.role='dialog'] - ARIA role
 * @param {string} [props.ariaLabel] - ARIA label
 * @param {boolean} [props.disableBackdropClick=false] - Disable closing on backdrop click
 * @param {boolean} [props.disableEscapeKey=false] - Disable closing on Escape key
 * @param {React.ReactNode} [props.footer] - Optional footer content
 * @param {React.ReactNode} props.children - Main dialog body
 */
export default function ModalWindow({
  onClose,
  title,
  tag,
  subtitle,
  meta,
  customTitleLeft,
  customTitleRight,
  allowMaximize = true,
  maximized: controlledMaximized,
  onMaximizedChange,
  defaultMaximized = false,
  width = 'min(1150px, 92vw)',
  height = 'min(760px, 88vh)',
  maxWidth,
  maxHeight,
  className = '',
  overlayClassName = '',
  role = 'dialog',
  ariaLabel,
  disableBackdropClick = false,
  disableEscapeKey = false,
  footer,
  children,
  style,
}) {
  const [uncontrolledMax, setUncontrolledMax] = useState(defaultMaximized);
  const isMax = controlledMaximized !== undefined ? controlledMaximized : uncontrolledMax;

  const toggleMax = useCallback(() => {
    const next = !isMax;
    if (controlledMaximized === undefined) {
      setUncontrolledMax(next);
    }
    onMaximizedChange?.(next);
  }, [isMax, controlledMaximized, onMaximizedChange]);

  useEffect(() => {
    if (disableEscapeKey || !onClose) return;
    const handleKeyDown = (e) => {
      if (e.key === 'Escape') {
        e.stopPropagation();
        onClose();
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [disableEscapeKey, onClose]);

  const dialogLabel = ariaLabel || (typeof title === 'string' ? title : 'Dialog');

  return (
    <div
      className={`modal-overlay ${overlayClassName}`}
      onClick={(e) => {
        if (!disableBackdropClick && e.target === e.currentTarget && onClose) {
          onClose();
        }
      }}
    >
      <div
        className={`modal-window ${isMax ? 'is-max' : ''} ${className}`}
        role={role}
        aria-modal="true"
        aria-label={dialogLabel}
        style={isMax ? undefined : { width, height, maxWidth, maxHeight, ...style }}
      >
        {/* ── Titlebar ── */}
        <div className="modal-titlebar">
          {customTitleLeft ? (
            customTitleLeft
          ) : (
            <div className="modal-title-left">
              {tag && (
                typeof tag === 'object' && !React.isValidElement(tag) ? (
                  <span
                    className="ve-tag modal-tag"
                    style={{
                      color: tag.color || 'var(--accent)',
                      background: tag.bg || 'rgba(127,217,154,0.10)',
                      borderColor: tag.border || 'rgba(127,217,154,0.30)',
                    }}
                  >
                    {tag.label}
                  </span>
                ) : (
                  <span className="ve-tag modal-tag">{tag}</span>
                )
              )}
              {title && <span className="modal-title">{title}</span>}
              {subtitle && <span className="ve-dim modal-subtitle">{subtitle}</span>}
              {meta && <span className="modal-meta">{meta}</span>}
            </div>
          )}

          {/* ── Titlebar Right Pinned Controls ── */}
          <div className="modal-title-right">
            {customTitleRight}
            {allowMaximize && (
              <button
                className="ve-close modal-btn-max"
                onClick={toggleMax}
                title={isMax ? 'Restore' : 'Maximise'}
                aria-label={isMax ? 'Restore' : 'Maximise'}
              >
                {isMax ? (
                  <svg width="13" height="13" viewBox="0 0 12 12" fill="none">
                    <rect x="1.5" y="3.5" width="7" height="7" stroke="currentColor" strokeWidth="1.2" fill="var(--bg-2)"/>
                    <rect x="3.5" y="1.5" width="7" height="7" stroke="currentColor" strokeWidth="1.2" fill="var(--bg-2)"/>
                  </svg>
                ) : (
                  <svg width="13" height="13" viewBox="0 0 12 12" fill="none">
                    <rect x="1.5" y="1.5" width="9" height="9" stroke="currentColor" strokeWidth="1.2"/>
                  </svg>
                )}
              </button>
            )}
            {onClose && (
              <button
                className="ve-close modal-btn-close"
                onClick={onClose}
                title="Close (Esc)"
                aria-label="Close"
              >
                ×
              </button>
            )}
          </div>
        </div>

        {/* ── Main Body ── */}
        <div className="modal-content">
          {children}
        </div>

        {/* ── Optional Footer ── */}
        {footer && (
          <div className="modal-footer">
            {footer}
          </div>
        )}
      </div>
    </div>
  );
}
