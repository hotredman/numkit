/**
 * Examples extraction and management for Numkit IDE.
 * Handles cloning examples into Virtual FS (/numkit_ide/examples/...) or
 * OS temporary directory on local disk.
 */

import tempFS from '../temporary.js';
import { getFileBaseName, getFileName } from './pathUtils.js';

export const BINARY_EXAMPLE_EXT = /\.(png|jpe?g|gif|bmp|tga|tiff?|webp|psd|hdr|pic|pgm|ppm|pnm|wav|mp3|m4a|ogg|flac|mat)$/i;

/**
 * Opens and extracts an example file into the active filesystem mode (virtual or local).
 *
 * @param {object} node - Tree node representing the example file.
 * @param {Array} tree - Entire examples tree (for folder resolution if needed).
 * @param {object} vfsAdapters - VFS adapters map.
 * @param {string} fsMode - 'virtual' | 'local'.
 * @returns {Promise<{content: string|null, vfsPath: string, targetDir: string, isBinary: boolean, fsMode: string, source: string}|null>}
 */
export async function openExample(node, tree, vfsAdapters, fsMode = 'virtual') {
  if (!node || node.type !== 'file' || !node._fetchPath) return null;

  const fname = getFileName(node.name || node.path) || 'example.m';
  const scriptBaseName = getFileBaseName(fname);
  const isBinary = BINARY_EXAMPLE_EXT.test(fname);
  let content = null;

  // Fetch only this specific file
  const filesToCopy = [];
  const mainRes = await fetch(node._fetchPath);
  if (!mainRes.ok) throw new Error(`fetch failed: ${node._fetchPath}`);

  if (isBinary) {
    const buf = await mainRes.arrayBuffer();
    filesToCopy.push({ name: fname, bytes: Array.from(new Uint8Array(buf)) });
  } else {
    content = await mainRes.text();
    filesToCopy.push({ name: fname, content });
  }

  const isElectron = typeof window !== 'undefined' && typeof window.nativeFS !== 'undefined';

  // ── Local Filesystem Mode (Electron Native) ──
  if (fsMode === 'local' && isElectron && typeof window.nativeFS.setupExample === 'function') {
    const targetDir = await window.nativeFS.setupExample(scriptBaseName, filesToCopy);
    return {
      content,
      vfsPath: `/${fname}`,
      targetDir,
      isBinary,
      fsMode: 'local',
      source: 'localFolder',
    };
  }

  // ── Virtual Filesystem Mode ──
  const targetRelDir = `/numkit_ide/examples/${scriptBaseName}`;
  const tempBackend = vfsAdapters?.temp || tempFS;

  if (tempBackend) {
    try {
      if (typeof tempBackend.mkdir === 'function') {
        await tempBackend.mkdir(targetRelDir);
      }
    } catch { /* ignore */ }

    for (const f of filesToCopy) {
      const p = `${targetRelDir}/${f.name}`;
      try {
        if (f.bytes && typeof tempBackend.writeFileBytes === 'function') {
          await tempBackend.writeFileBytes(p, new Uint8Array(f.bytes));
        } else if (f.content != null && typeof tempBackend.writeFile === 'function') {
          await tempBackend.writeFile(p, f.content);
        }
      } catch { /* ignore */ }
    }
  }

  const vfsPath = `${targetRelDir}/${fname}`;
  return {
    content,
    vfsPath,
    targetDir: targetRelDir,
    isBinary,
    fsMode: 'virtual',
    source: 'temporary',
  };
}
