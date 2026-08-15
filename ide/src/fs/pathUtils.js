/**
 * Pure path utility functions for Numkit IDE.
 * Handles normalization, sanitization, and parent resolution for both
 * Virtual File System (POSIX /) and Local File System (OS paths).
 */

/**
 * Checks if a path is a Windows drive path (e.g. C:, C:\, C:/, C:\Users, etc.).
 */
export function isLocalDiskPath(p) {
  return typeof p === 'string' && /^[A-Za-z]:([\\/]|$)/.test(p.trim());
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

  // Strip Windows drive letters (e.g. 'C:/...' or '/C:/...' or 'C:')
  const winMatch = p.match(/^(\/?[A-Za-z]:)(.*)$/);
  if (winMatch) {
    const afterDrive = winMatch[2];
    const tempIdx = afterDrive.indexOf('/temporary');
    if (tempIdx >= 0) {
      p = afterDrive.slice(tempIdx + '/temporary'.length);
    } else {
      p = afterDrive || '/';
    }
  }

  // Collapse multiple slashes
  p = p.replace(/\/+/g, '/');
  if (!p.startsWith('/')) p = '/' + p;
  if (p.length > 1 && p.endsWith('/')) p = p.slice(0, -1);
  return p || '/';
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
  let p = rawPath.trim();

  // If starts with Windows drive letter (e.g. 'c:', 'C:\', 'D:/foo')
  if (/^[A-Za-z]:([\\/]|$)/.test(p)) {
    const driveLetter = p[0].toUpperCase();
    const rest = p.slice(2).replace(/\//g, '\\');
    if (!rest || rest === '\\') {
      return `${driveLetter}:\\`;
    }
    let norm = `${driveLetter}:${rest.startsWith('\\') ? rest : '\\' + rest}`;
    if (norm.length > 3 && norm.endsWith('\\')) norm = norm.slice(0, -1);
    return norm;
  }

  // Relative path against localRoot
  if (localRoot && isLocalDiskPath(localRoot)) {
    let cleanRel = p.replace(/\//g, '\\');
    if (cleanRel.startsWith('\\')) cleanRel = cleanRel.slice(1);
    const normRoot = sanitizeLocalPath(localRoot);
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
    const idx = norm.lastIndexOf('\\');
    if (idx <= 2) {
      return norm.slice(0, 2) + '\\'; // 'C:\Users' -> 'C:\'
    }
    return norm.slice(0, idx); // 'C:\Users\Foo' -> 'C:\Users'
  }

  const norm = sanitizeVfsPath(p);
  const idx = norm.lastIndexOf('/');
  if (idx <= 0) return '/';
  return norm.slice(0, idx);
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
  return name.replace(/\.[^/.]+$/, '');
}
