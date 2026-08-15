import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { openExample, BINARY_EXAMPLE_EXT } from './examples';

describe('examples', () => {
  describe('BINARY_EXAMPLE_EXT', () => {
    it('matches binary media and dataset extensions', () => {
      expect(BINARY_EXAMPLE_EXT.test('image.png')).toBe(true);
      expect(BINARY_EXAMPLE_EXT.test('photo.jpg')).toBe(true);
      expect(BINARY_EXAMPLE_EXT.test('audio.wav')).toBe(true);
      expect(BINARY_EXAMPLE_EXT.test('data.mat')).toBe(true);
    });

    it('does not match source code files', () => {
      expect(BINARY_EXAMPLE_EXT.test('script.m')).toBe(false);
      expect(BINARY_EXAMPLE_EXT.test('data.csv')).toBe(false);
      expect(BINARY_EXAMPLE_EXT.test('notes.txt')).toBe(false);
    });
  });

  describe('openExample', () => {
    const originalFetch = global.fetch;

    beforeEach(() => {
      global.fetch = vi.fn();
    });

    afterEach(() => {
      global.fetch = originalFetch;
      delete window.nativeFS;
    });

    it('returns null for non-file or invalid node', async () => {
      expect(await openExample(null)).toBe(null);
      expect(await openExample({ type: 'folder' })).toBe(null);
      expect(await openExample({ type: 'file' })).toBe(null);
    });

    it('extracts text script into Virtual FS /numkit_ide/examples/<scriptBaseName>', async () => {
      const mockText = '% arithmetic example\na = 1 + 2;\n';
      global.fetch.mockResolvedValueOnce({
        ok: true,
        text: async () => mockText,
      });

      const mockVfs = {
        mkdir: vi.fn().mockResolvedValue(),
        writeFile: vi.fn().mockResolvedValue(),
      };

      const node = {
        type: 'file',
        name: 'arithmetic.m',
        _fetchPath: '/assets/examples/Basics/arithmetic.m',
      };

      const result = await openExample(node, [], { temp: mockVfs }, 'virtual');

      expect(result).toEqual({
        content: mockText,
        vfsPath: '/numkit_ide/examples/arithmetic/arithmetic.m',
        targetDir: '/numkit_ide/examples/arithmetic',
        isBinary: false,
        fsMode: 'virtual',
        source: 'temporary',
      });

      expect(mockVfs.mkdir).toHaveBeenCalledWith('/numkit_ide/examples/arithmetic');
      expect(mockVfs.writeFile).toHaveBeenCalledWith('/numkit_ide/examples/arithmetic/arithmetic.m', mockText);
    });

    it('delegates to nativeFS.setupExample in Electron Local mode', async () => {
      const mockText = '% nested_loops\nfor i=1:10, end\n';
      global.fetch.mockResolvedValueOnce({
        ok: true,
        text: async () => mockText,
      });

      window.nativeFS = {
        setupExample: vi.fn().mockResolvedValue('C:\\Users\\User\\AppData\\Local\\Temp\\numkit\\examples\\nested_loops'),
      };

      const node = {
        type: 'file',
        name: 'nested_loops.m',
        _fetchPath: '/assets/examples/Basics/nested_loops.m',
      };

      const result = await openExample(node, [], null, 'local');

      expect(window.nativeFS.setupExample).toHaveBeenCalledWith(
        'nested_loops',
        [{ name: 'nested_loops.m', content: mockText }]
      );

      expect(result).toEqual({
        content: mockText,
        vfsPath: 'C:\\Users\\User\\AppData\\Local\\Temp\\numkit\\examples\\nested_loops\\nested_loops.m',
        targetDir: 'C:\\Users\\User\\AppData\\Local\\Temp\\numkit\\examples\\nested_loops',
        isBinary: false,
        fsMode: 'local',
        source: 'localFolder',
      });
    });
  });
});
