import { describe, it, expect } from 'vitest';
import {
  isLocalDiskPath,
  sanitizeVfsPath,
  getParentDir,
  getFileName,
  getFileBaseName,
} from './pathUtils';

describe('pathUtils', () => {
  describe('isLocalDiskPath', () => {
    it('detects Windows absolute paths with backslashes and forward slashes', () => {
      expect(isLocalDiskPath('C:\\Users\\User\\project')).toBe(true);
      expect(isLocalDiskPath('c:/Users/User/project')).toBe(true);
      expect(isLocalDiskPath('D:\\Data')).toBe(true);
    });

    it('returns false for POSIX, relative, or empty paths', () => {
      expect(isLocalDiskPath('/numkit_ide/examples')).toBe(false);
      expect(isLocalDiskPath('/')).toBe(false);
      expect(isLocalDiskPath('examples/foo')).toBe(false);
      expect(isLocalDiskPath('')).toBe(false);
      expect(isLocalDiskPath(null)).toBe(false);
    });
  });

  describe('sanitizeVfsPath', () => {
    it('normalizes simple virtual paths', () => {
      expect(sanitizeVfsPath('/')).toBe('/');
      expect(sanitizeVfsPath('/foo/bar')).toBe('/foo/bar');
      expect(sanitizeVfsPath('foo/bar')).toBe('/foo/bar');
      expect(sanitizeVfsPath('//foo///bar//')).toBe('/foo/bar');
      expect(sanitizeVfsPath('   /a/b/c   ')).toBe('/a/b/c');
    });

    it('strips accidental host Windows drive prefixes', () => {
      expect(sanitizeVfsPath('C:/Users/User/AppData/Roaming/numkit_ide/temporary/examples/nested_loops')).toBe('/examples/nested_loops');
      expect(sanitizeVfsPath('/C:/temporary/numkit_ide/examples/arithmetic')).toBe('/numkit_ide/examples/arithmetic');
      expect(sanitizeVfsPath('C:\\something\\else')).toBe('/something/else');
    });

    it('handles empty or non-string inputs safely', () => {
      expect(sanitizeVfsPath('')).toBe('/');
      expect(sanitizeVfsPath(null)).toBe('/');
      expect(sanitizeVfsPath(undefined)).toBe('/');
    });
  });

  describe('getParentDir', () => {
    it('navigates up in Virtual FS mode', () => {
      expect(getParentDir('/numkit_ide/examples/nested_loops', false)).toBe('/numkit_ide/examples');
      expect(getParentDir('/numkit_ide/examples', false)).toBe('/numkit_ide');
      expect(getParentDir('/numkit_ide', false)).toBe('/');
      expect(getParentDir('/', false)).toBe('/');
    });

    it('navigates up in Local Windows FS mode', () => {
      expect(getParentDir('C:\\Users\\User\\Projects\\test', true)).toBe('C:\\Users\\User\\Projects');
      expect(getParentDir('C:\\Users\\User', true)).toBe('C:\\Users');
      expect(getParentDir('C:\\Users', true)).toBe('C:\\');
      expect(getParentDir('C:\\', true)).toBe('C:\\');
    });

    it('handles root edge cases safely', () => {
      expect(getParentDir('', false)).toBe('/');
      expect(getParentDir('', true)).toBe('');
      expect(getParentDir(null, false)).toBe('/');
    });
  });

  describe('getFileName and getFileBaseName', () => {
    it('extracts filename with extension', () => {
      expect(getFileName('/foo/bar/script.m')).toBe('script.m');
      expect(getFileName('C:\\Users\\script.m')).toBe('script.m');
      expect(getFileName('simple.m')).toBe('simple.m');
      expect(getFileName('')).toBe('');
    });

    it('extracts base name without extension', () => {
      expect(getFileBaseName('/foo/bar/script.m')).toBe('script');
      expect(getFileBaseName('C:\\Users\\image.png')).toBe('image');
      expect(getFileBaseName('archive.tar.gz')).toBe('archive.tar');
      expect(getFileBaseName('noext')).toBe('noext');
    });
  });
});
