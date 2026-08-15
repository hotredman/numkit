import { describe, it, expect } from 'vitest';
import {
  isLocalDiskPath,
  sanitizeVfsPath,
  sanitizeLocalPath,
  getParentDir,
  getFileName,
  getFileBaseName,
  getTabPaths,
} from './pathUtils';

describe('pathUtils', () => {
  describe('isLocalDiskPath', () => {
    it('detects Windows absolute paths with backslashes and forward slashes', () => {
      expect(isLocalDiskPath('C:\\Users\\User\\project')).toBe(true);
      expect(isLocalDiskPath('c:/Users/User/project')).toBe(true);
      expect(isLocalDiskPath('D:\\Data')).toBe(true);
      expect(isLocalDiskPath('C:')).toBe(true);
      expect(isLocalDiskPath('c:')).toBe(true);
      expect(isLocalDiskPath('C:\\')).toBe(true);
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

  describe('sanitizeLocalPath', () => {
    it('normalizes bare drive letters to drive root with trailing backslash', () => {
      expect(sanitizeLocalPath('C:')).toBe('C:\\');
      expect(sanitizeLocalPath('c:')).toBe('C:\\');
      expect(sanitizeLocalPath('C:\\')).toBe('C:\\');
      expect(sanitizeLocalPath('c:/')).toBe('C:\\');
    });

    it('navigates to the drive root when / or \\ is entered', () => {
      expect(sanitizeLocalPath('/', 'C:\\Users\\User')).toBe('C:\\');
      expect(sanitizeLocalPath('\\', 'C:\\Users\\User')).toBe('C:\\');
      expect(sanitizeLocalPath('/', 'D:\\Projects\\Data')).toBe('D:\\');
      expect(sanitizeLocalPath('/', '')).toBe('C:\\');
    });

    it('normalizes Windows drive paths with forward slashes and removes trailing slash', () => {
      expect(sanitizeLocalPath('c:/Users/User/Projects')).toBe('C:\\Users\\User\\Projects');
      expect(sanitizeLocalPath('C:\\Users\\User\\Projects\\')).toBe('C:\\Users\\User\\Projects');
      expect(sanitizeLocalPath('D:/Data/')).toBe('D:\\Data');
    });

    it('resolves relative paths against localRoot', () => {
      expect(sanitizeLocalPath('subfolder', 'C:\\Users\\User')).toBe('C:\\Users\\User\\subfolder');
      expect(sanitizeLocalPath('/subfolder', 'C:\\Users\\User')).toBe('C:\\Users\\User\\subfolder');
      expect(sanitizeLocalPath('\\subfolder', 'C:\\Users\\User')).toBe('C:\\Users\\User\\subfolder');
      expect(sanitizeLocalPath('nested/dir', 'C:\\')).toBe('C:\\nested\\dir');
    });

    it('handles empty inputs safely', () => {
      expect(sanitizeLocalPath('', 'C:\\Users')).toBe('C:\\Users');
      expect(sanitizeLocalPath(null, 'C:\\Users')).toBe('C:\\Users');
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
      expect(getParentDir('C:', true)).toBe('C:\\');
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

  describe('getTabPaths', () => {
    it('resolves paths for local folder tab with root-relative vfsPath', () => {
      const tab = {
        name: 'group_aggregation.m',
        vfsPath: '/group_aggregation.m',
        source: 'localFolder',
      };
      const res = getTabPaths(tab, 'C:\\Users\\User\\Temp\\group_aggregation');
      expect(res).toEqual({
        fileName: 'group_aggregation.m',
        filePath: 'C:\\Users\\User\\Temp\\group_aggregation\\group_aggregation.m',
        folderPath: 'C:\\Users\\User\\Temp\\group_aggregation',
        mode: 'local',
      });
    });

    it('resolves paths for local folder tab in subfolder', () => {
      const tab = {
        name: 'helper.m',
        vfsPath: '/models/helper.m',
        source: 'localFolder',
      };
      const res = getTabPaths(tab, 'C:\\Projects\\demo');
      expect(res).toEqual({
        fileName: 'helper.m',
        filePath: 'C:\\Projects\\demo\\models\\helper.m',
        folderPath: 'C:\\Projects\\demo\\models',
        mode: 'local',
      });
    });

    it('resolves paths for virtual temporary tab', () => {
      const tab = {
        name: 'arithmetic.m',
        vfsPath: '/numkit_ide/examples/arithmetic/arithmetic.m',
        source: 'temporary',
      };
      const res = getTabPaths(tab, '');
      expect(res).toEqual({
        fileName: 'arithmetic.m',
        filePath: '/numkit_ide/examples/arithmetic/arithmetic.m',
        folderPath: '/numkit_ide/examples/arithmetic',
        mode: 'virtual',
      });
    });

    it('resolves audio roundtrip example in Virtual FS mode without host disk leaks', () => {
      const tab = {
        name: 'audio_io_roundtrip.m',
        vfsPath: '/numkit_ide/examples/audio_io_roundtrip/audio_io_roundtrip.m',
        source: 'temporary',
      };
      const res = getTabPaths(tab, 'C:\\Users\\User\\Projects');
      expect(res).toEqual({
        fileName: 'audio_io_roundtrip.m',
        filePath: '/numkit_ide/examples/audio_io_roundtrip/audio_io_roundtrip.m',
        folderPath: '/numkit_ide/examples/audio_io_roundtrip',
        mode: 'virtual',
      });
    });

    it('resolves tab with absolute local Windows path', () => {
      const tab = {
        name: 'model.m',
        vfsPath: 'C:\\Data\\Projects\\model.m',
        source: 'localFolder',
      };
      const res = getTabPaths(tab, 'C:\\Data');
      expect(res).toEqual({
        fileName: 'model.m',
        filePath: 'C:\\Data\\Projects\\model.m',
        folderPath: 'C:\\Data\\Projects',
        mode: 'local',
      });
    });

    it('resolves tab at Windows drive root', () => {
      const tab = {
        name: 'root.m',
        vfsPath: '/root.m',
        source: 'localFolder',
      };
      const res = getTabPaths(tab, 'C:\\');
      expect(res).toEqual({
        fileName: 'root.m',
        filePath: 'C:\\root.m',
        folderPath: 'C:\\',
        mode: 'local',
      });
    });

    it('handles empty or null tab safely', () => {
      expect(getTabPaths(null)).toEqual({
        fileName: '',
        filePath: '',
        folderPath: '',
        mode: 'virtual',
      });
    });
  });
});
