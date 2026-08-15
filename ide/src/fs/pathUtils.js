/**
 * Pure path utility functions for Numkit IDE.
 * Handles normalization, sanitization, and parent resolution for both
 * Virtual File System (POSIX /) and Local File System (OS paths)
 * using deterministic string operations without complex regular expressions.
 */

/**
 * Checks if character code is an ASCII letter (A-Z or a-z).
 */
function isAsciiLetter(code) {
  return (code >= 65 && code <= 90) || (code >= 97 && code <= 122);
}

/**
 * Checks if a path starts with a Windows drive letter (e.g. "C:", "C:\", "c:/", "D:\Users").
 */
export function isLocalDiskPath(p) {
  if (!p || typeof p !== 'string') return false;
  const s = p.trim();
  if (s.length < 2) return false;
  if (!isAsciiLetter(s.charCodeAt(0)) || s[1] !== ':') return false;
  if (s.length === 2) return true; // e.g. "C:"
  return s[2] === '\\' || s[2] === '/';
}

/**
 * Normalizes and sanitizes a Virtual File System path:
 * - Always starts with '/'
 * - Forward slashes only
 * - Strips any Windows drive letters or '/temporary' prefixes accidentally leaking from disk roots
 * - No trailing slash (unless root '/')
 */
export function sanitizeVfsPath(rawPath) {
  if (!rawPath || typeof rawPath !== 'string') return '/';
  let p = rawPath.trim().replace(/\\/g, '/');

  // Strip host disk remnants if present
  const tempIdx = p.indexOf('/temporary');
  if (tempIdx >= 0) {
    p = p.slice(tempIdx + '/temporary'.length);
  } else if (p.length >= 2 && isAsciiLetter(p.charCodeAt(0)) && p[1] === ':') {
    // "C:/foo" -> "/foo"
    p = p.slice(2);
  } else if (p.length >= 3 && p[0] === '/' && isAsciiLetter(p.charCodeAt(1)) && p[2] === ':') {
    // "/C:/foo" -> "/foo"
    p = p.slice(3);
  }

  // Split into segments, filter empty parts, and re-join with '/'
  const segments = p.split('/').filter(Boolean);
  return '/' + segments.join('/');
}

/**
 * Normalizes a local filesystem path:
 * - If user entered 'C:' or 'c:', converts to 'C:\'
 * - If user entered 'C:/', converts to 'C:\'
 * - If user entered 'C:\Users\', converts to 'C:\Users' (unless root 'C:\')
 * - Replaces forward slashes with backslashes on Windows drive paths
 * - Resolves relative paths against localRoot if provided
 */
export function sanitizeLocalPath(rawPath, localRoot = '') {
  if (!rawPath || typeof rawPath !== 'string') return localRoot || '';
  const p = rawPath.trim();
  if (!p) return localRoot || '';

  if (isLocalDiskPath(p)) {
    const drive = p[0].toUpperCase() + ':';
    const rest = p.slice(2).replace(/\//g, '\\');
    // If bare drive "C:" or "C:\"
    if (!rest || rest === '\\') {
      return `${drive}\\`;
    }
    const withSlash = rest.startsWith('\\') ? rest : `\\${rest}`;
    // Strip trailing backslash unless root "C:\"
    return (withSlash.length > 1 && withSlash.endsWith('\\'))
      ? `${drive}${withSlash.slice(0, -1)}`
      : `${drive}${withSlash}`;
  }

  // Relative path against localRoot
  if (localRoot && isLocalDiskPath(localRoot)) {
    const normRoot = sanitizeLocalPath(localRoot);
    let cleanRel = p.replace(/\//g, '\\');
    while (cleanRel.startsWith('\\')) cleanRel = cleanRel.slice(1);
    while (cleanRel.endsWith('\\')) cleanRel = cleanRel.slice(0, -1);
    if (!cleanRel) return normRoot;
    return normRoot.endsWith('\\') ? `${normRoot}${cleanRel}` : `${normRoot}\\${cleanRel}`;
  }

  return p.replace(/\\/g, '/');
}

/**
 * Resolves the parent directory of a path.
 * In Local mode (Windows), preserves drive roots (e.g. 'C:\').
 * In Virtual mode, stays rooted at '/'.
 */
export function getParentDir(p, isLocal = false) {
  if (!p || typeof p !== 'string') return isLocal ? '' : '/';

  if (isLocal && isLocalDiskPath(p)) {
    const norm = sanitizeLocalPath(p);
    if (norm.length <= 3) {
      return norm; // 'C:\' stays 'C:\'
    }
    const lastSlash = norm.lastIndexOf('\\');
    if (lastSlash <= 2) {
      return norm.slice(0, 2) + '\\'; // 'C:\Users' -> 'C:\'
    }
    return norm.slice(0, lastSlash); // 'C:\Users\Foo' -> 'C:\Users'
  }

  const norm = sanitizeVfsPath(p);
  const lastSlash = norm.lastIndexOf('/');
  if (lastSlash <= 0) return '/';
  return norm.slice(0, lastSlash);
}

/**
 * Extracts the file name (with extension) from a path.
 */
export function getFileName(p) {
  if (!p || typeof p !== 'string') return '';
  const lastSlash = Math.max(p.lastIndexOf('/'), p.lastIndexOf('\\'));
  return lastSlash >= 0 ? p.slice(lastSlash + 1) : p;
}

/**
 * Extracts the base name (without extension) from a path or file name.
 */
export function getFileBaseName(p) {
  const name = getFileName(p);
  const dotIdx = name.lastIndexOf('.');
  return dotIdx > 0 ? name.slice(0, dotIdx) : name;
}
