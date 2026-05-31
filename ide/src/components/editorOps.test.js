// editorOps.test.js — vitest unit coverage for the pure-logic editor
// operations. Each test exercises one branch of one function on a
// minimal text fixture; the (text, selStart, selEnd) → (text,
// selStart, selEnd) contract makes assertions trivial to write.

import { describe, it, expect } from 'vitest';
import {
  INDENT, INDENT_LEN,
  applyTab, applyEnter,
  applyAutoClose, applyAutoSkip, applyAutoDeletePair,
  applySmartDedent,
  applyCommentToggle, applyDuplicateLine, applyMoveLine,
  trimTrailingWhitespace,
  lineStartAt, lineEndAt, leadingWhitespace,
  buildSearchRegex, findAllMatches, findNextMatch, findPrevMatch,
  applyReplace, applyReplaceAll, gotoLineOffset,
  findMatchingBracket, findWordOccurrences,
  applyDragDrop,
  partialBeforeCaret, applyCompletion, wordAt,
  computeIndentGuides,
} from './editorOps';

// ── helpers ────────────────────────────────────────────────────────

/** Compact builder: text with `|` marking caret, `[…]` marking selection. */
function buf(spec) {
  // Selection form: "foo[bar]baz" → text "foobarbaz", sel [3, 6].
  const selOpen = spec.indexOf('[');
  if (selOpen !== -1) {
    const selClose = spec.indexOf(']', selOpen + 1);
    const text = spec.slice(0, selOpen) + spec.slice(selOpen + 1, selClose) + spec.slice(selClose + 1);
    return { text, selStart: selOpen, selEnd: selClose - 1 };
  }
  // Caret form: "foo|bar" → text "foobar", caret at 3.
  const caret = spec.indexOf('|');
  if (caret !== -1) {
    return { text: spec.replace('|', ''), selStart: caret, selEnd: caret };
  }
  return { text: spec, selStart: 0, selEnd: 0 };
}

// ── primitives ─────────────────────────────────────────────────────

describe('lineStartAt / lineEndAt / leadingWhitespace', () => {
  it('lineStartAt at start of text → 0', () => {
    expect(lineStartAt('abc', 0)).toBe(0);
  });
  it('lineStartAt after newline → char after \\n', () => {
    expect(lineStartAt('abc\ndef', 5)).toBe(4);
  });
  it('lineEndAt finds next newline', () => {
    expect(lineEndAt('abc\ndef', 1)).toBe(3);
  });
  it('lineEndAt without trailing newline returns text length', () => {
    expect(lineEndAt('abc', 1)).toBe(3);
  });
  it('leadingWhitespace counts spaces only', () => {
    expect(leadingWhitespace('    foo')).toBe('    ');
    expect(leadingWhitespace('foo')).toBe('');
    expect(leadingWhitespace('\tfoo')).toBe('\t');
  });
});

// ── applyTab ───────────────────────────────────────────────────────

describe('applyTab', () => {
  it('inserts 4 spaces at caret (no selection)', () => {
    const { text, selStart, selEnd } = buf('foo|bar');
    const r = applyTab(text, selStart, selEnd, false);
    expect(r.text).toBe('foo    bar');
    expect(r.selStart).toBe(7);
    expect(r.selEnd).toBe(7);
  });

  it('replaces single-line selection with 4 spaces (MATLAB-Editor semantics)', () => {
    const { text, selStart, selEnd } = buf('foo[bar]baz');
    const r = applyTab(text, selStart, selEnd, false);
    expect(r.text).toBe('foo    baz');
    expect(r.selStart).toBe(7);
    expect(r.selEnd).toBe(7);
  });

  it('indents every line in a multi-line selection', () => {
    const text = 'a\nb\nc';
    // select 'a\nb\n' → covers lines 1 + 2; line-3 untouched
    const r = applyTab(text, 0, 4, false);
    expect(r.text).toBe('    a\n    b\nc');
  });

  it('indents the line containing the caret when selEnd lands mid-line', () => {
    const text = 'foo\nbar\nbaz';
    // selection covers 'oo\nb' → lines 1+2 both fully indented
    const r = applyTab(text, 1, 5, false);
    expect(r.text).toBe('    foo\n    bar\nbaz');
  });

  it('does NOT extend into next line when selection ends right at \\n', () => {
    const text = 'a\nb\nc';
    // selection 'a\n' (ends at \n boundary, exclusive of line 2)
    const r = applyTab(text, 0, 2, false);
    expect(r.text).toBe('    a\nb\nc');
  });

  it('Shift+Tab outdents a single line (no selection)', () => {
    const { text, selStart, selEnd } = buf('    fo|o');
    const r = applyTab(text, selStart, selEnd, true);
    expect(r.text).toBe('foo');
    expect(r.selStart).toBe(2);   // 'fo' → caret on 'o' (col 3 → 2 after −4 leading)
  });

  it('Shift+Tab outdents every line in selection', () => {
    const text = '    a\n    b\n    c';
    const r = applyTab(text, 0, text.length, true);
    expect(r.text).toBe('a\nb\nc');
  });

  it('Shift+Tab is no-op on lines without leading whitespace', () => {
    const { text, selStart, selEnd } = buf('foo|');
    const r = applyTab(text, selStart, selEnd, true);
    expect(r.text).toBe('foo');
    expect(r.selStart).toBe(3);
  });

  it('Shift+Tab strips ≤4 spaces (not more) per line', () => {
    const text = '        foo';   // 8 leading spaces
    const r = applyTab(text, 0, 0, true);
    expect(r.text).toBe('    foo');
  });

  it('caret clamps to line start when outdenting from inside leading whitespace', () => {
    const { text, selStart, selEnd } = buf('  |  foo');   // caret in middle of 4 spaces
    const r = applyTab(text, selStart, selEnd, true);
    expect(r.text).toBe('foo');
    expect(r.selStart).toBe(0);   // clamped to line start
  });
});

// ── applyEnter ─────────────────────────────────────────────────────

describe('applyEnter', () => {
  it('inserts plain newline at end of unindented line', () => {
    const { text, selStart, selEnd } = buf('foo|');
    const r = applyEnter(text, selStart, selEnd);
    expect(r.text).toBe('foo\n');
    expect(r.selStart).toBe(4);
  });

  it('inherits leading whitespace of previous line', () => {
    const { text, selStart, selEnd } = buf('    foo|');
    const r = applyEnter(text, selStart, selEnd);
    expect(r.text).toBe('    foo\n    ');
    expect(r.selStart).toBe(12);
  });

  it('adds extra indent after `for`', () => {
    const { text, selStart, selEnd } = buf('for i = 1:10|');
    const r = applyEnter(text, selStart, selEnd);
    expect(r.text).toBe('for i = 1:10\n    ');
  });

  it('adds extra indent after `if`', () => {
    const { text, selStart, selEnd } = buf('if x > 0|');
    const r = applyEnter(text, selStart, selEnd);
    expect(r.text).toBe('if x > 0\n    ');
  });

  it('compounds: indented `for` produces double-indent newline', () => {
    const { text, selStart, selEnd } = buf('    for k = 1:n|');
    const r = applyEnter(text, selStart, selEnd);
    expect(r.text).toBe('    for k = 1:n\n        ');
  });

  it('honours every documented block-opener', () => {
    for (const kw of ['for', 'parfor', 'while', 'if', 'switch', 'try',
                      'function', 'classdef', 'else', 'elseif',
                      'case', 'otherwise', 'catch']) {
      const text = `${kw} foo`;
      const r = applyEnter(text, text.length, text.length);
      expect(r.text).toBe(`${kw} foo\n    `);
    }
  });

  it('does NOT add extra indent after a comment that begins with `for`', () => {
    const { text, selStart, selEnd } = buf('% for example|');
    const r = applyEnter(text, selStart, selEnd);
    expect(r.text).toBe('% for example\n');
  });

  it('does NOT add extra indent on a non-opener line', () => {
    const { text, selStart, selEnd } = buf('y = 2 * x|');
    const r = applyEnter(text, selStart, selEnd);
    expect(r.text).toBe('y = 2 * x\n');
  });

  it('replaces an active selection before inserting newline', () => {
    const { text, selStart, selEnd } = buf('hel[lo wo]rld');
    const r = applyEnter(text, selStart, selEnd);
    expect(r.text).toBe('hel\nrld');
  });
});

// ── applyAutoClose ─────────────────────────────────────────────────

describe('applyAutoClose', () => {
  it('inserts a pair and places caret between for `(`', () => {
    const { text, selStart, selEnd } = buf('foo|');
    const r = applyAutoClose(text, selStart, selEnd, '(');
    expect(r.text).toBe('foo()');
    expect(r.selStart).toBe(4);
    expect(r.selEnd).toBe(4);
  });

  it('handles `[`', () => {
    const r = applyAutoClose('', 0, 0, '[');
    expect(r.text).toBe('[]');
  });

  it('handles `{`', () => {
    const r = applyAutoClose('', 0, 0, '{');
    expect(r.text).toBe('{}');
  });

  it('handles `"`', () => {
    const r = applyAutoClose('', 0, 0, '"');
    expect(r.text).toBe('""');
  });

  it('wraps an active selection in brackets', () => {
    const { text, selStart, selEnd } = buf('hello [world] !');
    const r = applyAutoClose(text, selStart, selEnd, '(');
    expect(r.text).toBe('hello (world) !');
    expect(r.selStart).toBe(7);
    expect(r.selEnd).toBe(12);
  });

  it("does NOT auto-close `'` after an identifier (transpose context)", () => {
    const { text, selStart, selEnd } = buf('x|');
    const r = applyAutoClose(text, selStart, selEnd, "'");
    expect(r).toBeNull();
  });

  it("does NOT auto-close `'` after `)`", () => {
    const { text, selStart, selEnd } = buf('foo()|');
    const r = applyAutoClose(text, selStart, selEnd, "'");
    expect(r).toBeNull();
  });

  it("auto-closes `'` at start of line (string context)", () => {
    const { text, selStart, selEnd } = buf(' |');
    const r = applyAutoClose(text, selStart, selEnd, "'");
    expect(r.text).toBe(" ''");
  });

  it('returns null for non-opener input', () => {
    expect(applyAutoClose('', 0, 0, 'x')).toBeNull();
  });
});

// ── applyAutoSkip ──────────────────────────────────────────────────

describe('applyAutoSkip', () => {
  it('moves caret past a matching close char', () => {
    const { text, selStart, selEnd } = buf('foo(|)');
    const r = applyAutoSkip(text, selStart, selEnd, ')');
    expect(r.text).toBe('foo()');
    expect(r.selStart).toBe(5);
    expect(r.selEnd).toBe(5);
  });

  it('null when next char does not match typed char', () => {
    const { text, selStart, selEnd } = buf('foo(|x)');
    expect(applyAutoSkip(text, selStart, selEnd, ')')).toBeNull();
  });

  it('null when there is a selection', () => {
    expect(applyAutoSkip('foo()', 3, 4, ')')).toBeNull();
  });

  it('null for non-closer chars', () => {
    expect(applyAutoSkip('xx', 0, 0, 'x')).toBeNull();
  });
});

// ── applyAutoDeletePair ────────────────────────────────────────────

describe('applyAutoDeletePair', () => {
  it('deletes both chars of an empty pair on backspace', () => {
    const { text, selStart, selEnd } = buf('foo(|)bar');
    const r = applyAutoDeletePair(text, selStart, selEnd);
    expect(r.text).toBe('foobar');
    expect(r.selStart).toBe(3);
  });

  it('handles every pair', () => {
    for (const [o, c] of [['(', ')'], ['[', ']'], ['{', '}'], ['"', '"'], ["'", "'"]]) {
      const r = applyAutoDeletePair(o + c, 1, 1);
      expect(r.text).toBe('');
      expect(r.selStart).toBe(0);
    }
  });

  it('null when caret has selection', () => {
    expect(applyAutoDeletePair('()', 0, 1)).toBeNull();
  });

  it('null when prev char is not an opener', () => {
    expect(applyAutoDeletePair('xx', 1, 1)).toBeNull();
  });

  it('null when next char is not the matching closer', () => {
    expect(applyAutoDeletePair('(x', 1, 1)).toBeNull();
  });
});

// ── applySmartDedent ───────────────────────────────────────────────

describe('applySmartDedent', () => {
  it('dedents a freshly-typed `end` on its own line', () => {
    // Setup: user just typed the 'd' of "end" on an indented line.
    const text = '    end';
    const r = applySmartDedent(text, text.length, text.length);
    expect(r.text).toBe('end');
    expect(r.selStart).toBe(3);
  });

  it('dedents `else`, `case`, `otherwise`, `catch`', () => {
    // `elseif` is excluded — its prefix `else` already fires the
    // dedent during letter-by-letter typing, see dedicated test below.
    for (const kw of ['else', 'case', 'otherwise', 'catch']) {
      const text = '        ' + kw;   // 8 leading spaces
      const r = applySmartDedent(text, text.length, text.length);
      expect(r.text).toBe('    ' + kw);
    }
  });

  it('no-op when leading whitespace < 4', () => {
    const text = '  end';   // only 2 spaces
    expect(applySmartDedent(text, text.length, text.length)).toBeNull();
  });

  it('no-op for non-dedent keywords', () => {
    const text = '    foo';
    expect(applySmartDedent(text, text.length, text.length)).toBeNull();
  });

  it('no-op when caret is not right after the keyword', () => {
    const text = '    end   ';
    // caret at end of trailing whitespace — keyword was typed earlier
    expect(applySmartDedent(text, text.length, text.length)).toBeNull();
  });

  it('caret follows the dedented keyword correctly', () => {
    const text = '    end';
    const r = applySmartDedent(text, text.length, text.length);
    expect(r.selStart).toBe(3);   // caret after 'end' on now-unindented line
  });

  it('only dedents the current line, leaves rest alone', () => {
    const text = 'foo\n    end\nbar';
    const lineStart = 4;
    const caret = lineStart + 7;   // end of "    end"
    const r = applySmartDedent(text, caret, caret);
    expect(r.text).toBe('foo\nend\nbar');
  });

  it('does NOT double-dedent when extending `else` → `elseif`', () => {
    // The dedent already fired on `else`; extending to `elseif`
    // must not pull the line back another step.
    const text = '    elseif';   // already at the post-`else`-dedent indent
    const r = applySmartDedent(text, text.length, text.length);
    expect(r).toBeNull();
  });
});

// ── trimTrailingWhitespace ─────────────────────────────────────────

describe('trimTrailingWhitespace', () => {
  it('removes trailing spaces from each line', () => {
    expect(trimTrailingWhitespace('foo   \nbar\t\n')).toBe('foo\nbar\n');
  });

  it('preserves empty lines and intra-line spaces', () => {
    expect(trimTrailingWhitespace('  foo  \n\n  bar')).toBe('  foo\n\n  bar');
  });

  it('handles no-trailing-newline input', () => {
    expect(trimTrailingWhitespace('foo   ')).toBe('foo');
  });

  it('is identity on already-clean input', () => {
    const clean = 'foo\nbar\nbaz';
    expect(trimTrailingWhitespace(clean)).toBe(clean);
  });
});

// ── applyCommentToggle ─────────────────────────────────────────────

describe('applyCommentToggle', () => {
  it('adds `% ` to a single uncommented line', () => {
    const { text, selStart, selEnd } = buf('foo|');
    const r = applyCommentToggle(text, selStart, selEnd);
    expect(r.text).toBe('% foo');
  });

  it('strips `% ` to uncomment a single commented line', () => {
    const { text, selStart, selEnd } = buf('% foo|');
    const r = applyCommentToggle(text, selStart, selEnd);
    expect(r.text).toBe('foo');
  });

  it('strips bare `%` (no space) when uncommenting', () => {
    const r = applyCommentToggle('%foo', 0, 0);
    expect(r.text).toBe('foo');
  });

  it('preserves indent — comment lands after the leading whitespace', () => {
    const r = applyCommentToggle('    foo', 0, 0);
    expect(r.text).toBe('    % foo');
  });

  it('preserves indent on uncomment too', () => {
    const r = applyCommentToggle('    % foo', 0, 0);
    expect(r.text).toBe('    foo');
  });

  it('toggles a multi-line selection — all uncommented → comment all', () => {
    const text = 'foo\nbar\nbaz';
    const r = applyCommentToggle(text, 0, text.length);
    expect(r.text).toBe('% foo\n% bar\n% baz');
  });

  it('toggles a multi-line selection — all commented → uncomment all', () => {
    const text = '% foo\n% bar\n% baz';
    const r = applyCommentToggle(text, 0, text.length);
    expect(r.text).toBe('foo\nbar\nbaz');
  });

  it('treats mixed (some commented, some not) as "add" — comments the uncommented ones', () => {
    const text = '% foo\nbar\n% baz';
    const r = applyCommentToggle(text, 0, text.length);
    // ALL get a `% ` prefix because we're in add mode (not all were commented).
    expect(r.text).toBe('% % foo\n% bar\n% % baz');
  });

  it('skips blank lines in both modes', () => {
    const text = 'foo\n\nbar';
    const r = applyCommentToggle(text, 0, text.length);
    expect(r.text).toBe('% foo\n\n% bar');
  });
});

// ── applyDuplicateLine ─────────────────────────────────────────────

describe('applyDuplicateLine', () => {
  it('clones the current line below (no selection)', () => {
    const { text, selStart, selEnd } = buf('foo|');
    const r = applyDuplicateLine(text, selStart, selEnd);
    expect(r.text).toBe('foo\nfoo');
    expect(r.selStart).toBe(7);   // caret on new line at same col
  });

  it('clones a middle line and preserves column', () => {
    const text = 'a\nbar\nc';
    // caret at position 5 = 'a' of 'bar' line + 1 (the 'a' in 'bar')
    // line 'bar' starts at index 2, caret at index 4 (after 'ba')
    const r = applyDuplicateLine(text, 4, 4);
    expect(r.text).toBe('a\nbar\nbar\nc');
    expect(r.selStart).toBe(8);   // 2 (line start of clone) + 2 (col offset 'ba')... actually 6+2=8
  });

  it('clones an active selection concatenated right after', () => {
    const { text, selStart, selEnd } = buf('hel[lo]world');
    // text='helloworld', sel=text[3..5]='lo'.
    // Duplicate inserts 'lo' at index 5 → 'hello'+'lo'+'world'.
    const r = applyDuplicateLine(text, selStart, selEnd);
    expect(r.text).toBe('helloloworld');
    expect(r.selStart).toBe(5);   // right after original selection
    expect(r.selEnd).toBe(7);     // covers the clone
  });

  it('clones the last line without trailing newline', () => {
    const text = 'foo';
    const r = applyDuplicateLine(text, 3, 3);
    expect(r.text).toBe('foo\nfoo');
  });
});

// ── applyMoveLine ──────────────────────────────────────────────────

describe('applyMoveLine', () => {
  it('moves a single line up', () => {
    const text = 'a\nb\nc';
    // caret on line 'b' (index 2..3)
    const r = applyMoveLine(text, 2, 2, -1);
    expect(r.text).toBe('b\na\nc');
    expect(r.selStart).toBe(0);
  });

  it('moves a single line down', () => {
    const text = 'a\nb\nc';
    // caret on line 'b' (index 2)
    const r = applyMoveLine(text, 2, 2, +1);
    expect(r.text).toBe('a\nc\nb');
    expect(r.selStart).toBe(4);   // 'b' now starts at index 4
  });

  it('no-op when moving the top line up', () => {
    expect(applyMoveLine('a\nb', 0, 0, -1)).toBeNull();
  });

  it('no-op when moving the bottom line down (no trailing newline)', () => {
    const text = 'a\nb';
    expect(applyMoveLine(text, 2, 2, +1)).toBeNull();
  });

  it('moves a block selection up', () => {
    const text = 'a\nb\nc\nd';
    // selection covers 'b\nc' (indices 2..5)
    const r = applyMoveLine(text, 2, 5, -1);
    expect(r.text).toBe('b\nc\na\nd');
    expect(r.selStart).toBe(0);
    expect(r.selEnd).toBe(3);
  });

  it('moves a block selection down', () => {
    const text = 'a\nb\nc\nd';
    const r = applyMoveLine(text, 2, 5, +1);
    expect(r.text).toBe('a\nd\nb\nc');
    // selection shifted by +len('d\n') = 2
    expect(r.selStart).toBe(4);
    expect(r.selEnd).toBe(7);
  });

  it('returns null for invalid direction', () => {
    expect(applyMoveLine('a\nb', 0, 0, 0)).toBeNull();
    expect(applyMoveLine('a\nb', 0, 0, +2)).toBeNull();
  });

  it('preserves caret offset within the moved line', () => {
    const text = 'aa\nbb\ncc';
    // caret in 'bb' at column 1 (index 4)
    const r = applyMoveLine(text, 4, 4, -1);
    expect(r.text).toBe('bb\naa\ncc');
    expect(r.selStart).toBe(1);   // still column 1 on the moved-up line
  });
});

// ── buildSearchRegex ───────────────────────────────────────────────

describe('buildSearchRegex', () => {
  it('returns null for empty / null query', () => {
    expect(buildSearchRegex('')).toBeNull();
    expect(buildSearchRegex(null)).toBeNull();
  });

  it('default is case-insensitive', () => {
    const re = buildSearchRegex('Foo');
    expect(re.flags).toContain('i');
    expect(re.flags).toContain('g');
    expect('foo bar Foo'.match(re).length).toBe(2);
  });

  it('caseSensitive flag', () => {
    const re = buildSearchRegex('Foo', { caseSensitive: true });
    expect(re.flags).not.toContain('i');
    expect('foo Foo FOO'.match(re).length).toBe(1);
  });

  it('escapes regex metacharacters in plain mode', () => {
    const re = buildSearchRegex('a.b', {});
    expect(re.source).toBe('a\\.b');
  });

  it('passes through valid regex in regex mode', () => {
    const re = buildSearchRegex('a.b', { regex: true });
    expect(re.source).toBe('a.b');
  });

  it('returns null on invalid regex', () => {
    expect(buildSearchRegex('(', { regex: true })).toBeNull();
  });

  it('wraps wholeWord with \\b boundaries', () => {
    const re = buildSearchRegex('foo', { wholeWord: true });
    expect(re.source).toBe('\\bfoo\\b');
    expect('foo foobar'.match(re).length).toBe(1);   // only the lone 'foo'
  });
});

// ── findAllMatches ─────────────────────────────────────────────────

describe('findAllMatches', () => {
  it('finds every occurrence', () => {
    const m = findAllMatches('abc abc abc', 'abc');
    expect(m).toEqual([
      { start: 0, end: 3 },
      { start: 4, end: 7 },
      { start: 8, end: 11 },
    ]);
  });

  it('returns [] for missing query', () => {
    expect(findAllMatches('abc', 'xyz')).toEqual([]);
  });

  it('handles overlapping potential matches (regex non-overlap)', () => {
    expect(findAllMatches('aaaa', 'aa').length).toBe(2);
  });

  it('avoids infinite loop on zero-width regex matches', () => {
    // /^/ would match every position; we step lastIndex to escape.
    const m = findAllMatches('abc', '^', { regex: true });
    expect(m.length).toBe(0);   // zero-width matches are skipped
  });
});

// ── findNextMatch / findPrevMatch ──────────────────────────────────

describe('findNextMatch / findPrevMatch', () => {
  it('next-from-zero returns first match', () => {
    expect(findNextMatch('abc abc', 0, 'abc')).toEqual({ start: 0, end: 3 });
  });

  it('next skips matches before fromOffset', () => {
    expect(findNextMatch('abc abc', 1, 'abc')).toEqual({ start: 4, end: 7 });
  });

  it('next wraps when no forward match', () => {
    expect(findNextMatch('abc abc', 5, 'abc')).toEqual({ start: 0, end: 3 });
  });

  it('prev returns previous match strictly before fromOffset', () => {
    expect(findPrevMatch('abc abc', 6, 'abc')).toEqual({ start: 0, end: 3 });
  });

  it('prev wraps when no backward match', () => {
    expect(findPrevMatch('abc abc', 1, 'abc')).toEqual({ start: 4, end: 7 });
  });

  it('returns null when nothing matches', () => {
    expect(findNextMatch('abc', 0, 'xyz')).toBeNull();
    expect(findPrevMatch('abc', 0, 'xyz')).toBeNull();
  });
});

// ── applyReplace / applyReplaceAll ─────────────────────────────────

describe('applyReplace', () => {
  it('replaces a match range with the new text', () => {
    const r = applyReplace('hello world', { start: 6, end: 11 }, 'there');
    expect(r.text).toBe('hello there');
    expect(r.selStart).toBe(6);
    expect(r.selEnd).toBe(11);
  });

  it('handles different-length replacement', () => {
    const r = applyReplace('foo bar', { start: 0, end: 3 }, 'baaz');
    expect(r.text).toBe('baaz bar');
    expect(r.selEnd).toBe(4);
  });
});

describe('applyReplaceAll', () => {
  it('replaces every occurrence and counts', () => {
    const r = applyReplaceAll('abc abc abc', 'abc', 'x');
    expect(r.text).toBe('x x x');
    expect(r.count).toBe(3);
  });

  it('returns count 0 when no matches', () => {
    const r = applyReplaceAll('abc', 'xyz', '_');
    expect(r.text).toBe('abc');
    expect(r.count).toBe(0);
  });

  it('respects caseSensitive option', () => {
    const r = applyReplaceAll('Foo foo FOO', 'foo', 'x', { caseSensitive: true });
    expect(r.text).toBe('Foo x FOO');
    expect(r.count).toBe(1);
  });

  it('respects wholeWord option', () => {
    const r = applyReplaceAll('foo foobar', 'foo', 'x', { wholeWord: true });
    expect(r.text).toBe('x foobar');
    expect(r.count).toBe(1);
  });

  it('returns input unchanged for empty query', () => {
    const r = applyReplaceAll('abc', '', 'x');
    expect(r.text).toBe('abc');
    expect(r.count).toBe(0);
  });
});

// ── gotoLineOffset ─────────────────────────────────────────────────

describe('gotoLineOffset', () => {
  it('line 1 → offset 0', () => {
    expect(gotoLineOffset('a\nb\nc', 1)).toEqual({ offset: 0, line: 1 });
  });

  it('line 2 lands after the first newline', () => {
    expect(gotoLineOffset('a\nb\nc', 2)).toEqual({ offset: 2, line: 2 });
  });

  it('line 3 lands after the second newline', () => {
    expect(gotoLineOffset('a\nb\nc', 3)).toEqual({ offset: 4, line: 3 });
  });

  it('clamps to last line when target is past the end', () => {
    const r = gotoLineOffset('a\nb\nc', 99);
    expect(r.line).toBe(3);
    expect(r.offset).toBe(4);
  });

  it('clamps to line 1 for non-positive input', () => {
    expect(gotoLineOffset('a\nb', 0)).toEqual({ offset: 0, line: 1 });
    expect(gotoLineOffset('a\nb', -5)).toEqual({ offset: 0, line: 1 });
  });
});

// ── findMatchingBracket ────────────────────────────────────────────

describe('findMatchingBracket', () => {
  it('matches `(` to its `)` (caret right before the `(`)', () => {
    expect(findMatchingBracket('foo(bar)', 3)).toEqual({ open: 3, close: 7 });
  });

  it('matches `)` to its `(` (caret right after the `)`)', () => {
    expect(findMatchingBracket('foo(bar)', 8)).toEqual({ open: 3, close: 7 });
  });

  it('respects nesting on open brackets', () => {
    // ((x))
    // 01234
    expect(findMatchingBracket('((x))', 0)).toEqual({ open: 0, close: 4 });
    expect(findMatchingBracket('((x))', 1)).toEqual({ open: 1, close: 3 });
  });

  it('handles `[` `]` and `{` `}`', () => {
    expect(findMatchingBracket('[a]', 0)).toEqual({ open: 0, close: 2 });
    expect(findMatchingBracket('{a}', 0)).toEqual({ open: 0, close: 2 });
  });

  it('returns null when caret is not adjacent to any bracket', () => {
    expect(findMatchingBracket('foo bar', 2)).toBeNull();
  });

  it('returns { open, close: -1 } for unmatched open bracket', () => {
    expect(findMatchingBracket('(foo', 0)).toEqual({ open: 0, close: -1 });
  });

  it('returns { open: -1, close } for unmatched close bracket', () => {
    expect(findMatchingBracket('foo)', 4)).toEqual({ open: -1, close: 3 });
  });
});

// ── findWordOccurrences ────────────────────────────────────────────

describe('findWordOccurrences', () => {
  it('finds every other whole-word occurrence of the selection', () => {
    const text = 'foo bar foo baz foo';
    // Select first 'foo' (indices 0..3)
    const r = findWordOccurrences(text, 0, 3);
    expect(r).toEqual([
      { start: 8, end: 11 },
      { start: 16, end: 19 },
    ]);
  });

  it('returns [] for empty selection', () => {
    expect(findWordOccurrences('foo foo', 0, 0)).toEqual([]);
  });

  it('returns [] for non-identifier selection (has space)', () => {
    expect(findWordOccurrences('foo bar', 0, 7)).toEqual([]);
  });

  it('returns [] for non-identifier selection (punctuation)', () => {
    expect(findWordOccurrences('foo.bar', 0, 7)).toEqual([]);
  });

  it('is case-sensitive (Foo ≠ foo)', () => {
    const text = 'Foo foo Foo';
    const r = findWordOccurrences(text, 0, 3);   // select first 'Foo'
    expect(r).toEqual([{ start: 8, end: 11 }]);
  });

  it('uses whole-word boundaries (foo doesn`t match foobar)', () => {
    const text = 'foo foobar foo';
    const r = findWordOccurrences(text, 0, 3);
    expect(r).toEqual([{ start: 11, end: 14 }]);   // only the standalone foo
  });
});

// ── applyDragDrop ──────────────────────────────────────────────────

describe('applyDragDrop', () => {
  it('moves a slice forward, adjusting dst for the deletion', () => {
    // text = 'hello world', move 'hello' (0..5) to position 11 (end)
    const r = applyDragDrop('hello world', 0, 5, 11, 'move');
    expect(r.text).toBe(' worldhello');
    expect(r.selStart).toBe(6);
    expect(r.selEnd).toBe(11);
  });

  it('moves a slice backward (no dst adjustment needed)', () => {
    const r = applyDragDrop('hello world', 6, 11, 0, 'move');
    expect(r.text).toBe('worldhello ');
    expect(r.selStart).toBe(0);
    expect(r.selEnd).toBe(5);
  });

  it('copies a slice without deleting the source', () => {
    const r = applyDragDrop('hello world', 0, 5, 11, 'copy');
    expect(r.text).toBe('hello worldhello');
    expect(r.selStart).toBe(11);
    expect(r.selEnd).toBe(16);
  });

  it('returns null when source is empty', () => {
    expect(applyDragDrop('foo', 1, 1, 0, 'move')).toBeNull();
  });

  it('returns null when dropping inside the source (move mode)', () => {
    expect(applyDragDrop('hello world', 0, 5, 3, 'move')).toBeNull();
  });

  it('accepts reversed srcStart/srcEnd', () => {
    // selectionEnd < selectionStart shouldn't break the op.
    const r = applyDragDrop('hello world', 5, 0, 11, 'move');
    expect(r.text).toBe(' worldhello');
  });

  it('returns null for invalid mode', () => {
    expect(applyDragDrop('foo', 0, 1, 2, 'nope')).toBeNull();
  });
});

// ── partialBeforeCaret ─────────────────────────────────────────────

describe('partialBeforeCaret', () => {
  it('extracts the word-prefix immediately before the caret', () => {
    expect(partialBeforeCaret('foo bar', 7)).toEqual({ start: 4, value: 'bar' });
  });

  it('returns empty value when caret is not on a word', () => {
    expect(partialBeforeCaret('foo ', 4)).toEqual({ start: 4, value: '' });
  });

  it('handles caret at start of text', () => {
    expect(partialBeforeCaret('abc', 3)).toEqual({ start: 0, value: 'abc' });
  });

  it('digits and underscores count as word chars', () => {
    expect(partialBeforeCaret('var_x12', 7)).toEqual({ start: 0, value: 'var_x12' });
  });
});

// ── applyCompletion ────────────────────────────────────────────────

describe('applyCompletion', () => {
  it('replaces the prefix with the completion item', () => {
    const r = applyCompletion('plo', 3, 'plot');
    expect(r.text).toBe('plot');
    expect(r.selStart).toBe(4);
  });

  it('preserves trailing text', () => {
    const r = applyCompletion('plo(x)', 3, 'plot');
    expect(r.text).toBe('plot(x)');
    expect(r.selStart).toBe(4);
  });

  it('returns null when there is no prefix', () => {
    expect(applyCompletion('foo ', 4, 'bar')).toBeNull();
  });
});

// ── wordAt ─────────────────────────────────────────────────────────

describe('wordAt', () => {
  it('returns word + range under caret in the middle of word', () => {
    expect(wordAt('hello world', 3)).toEqual({ start: 0, end: 5, word: 'hello' });
  });

  it('handles caret at the END of a word (right boundary)', () => {
    expect(wordAt('hello world', 5)).toEqual({ start: 0, end: 5, word: 'hello' });
  });

  it('returns null on whitespace', () => {
    expect(wordAt('hello world', 6)).toEqual({ start: 6, end: 11, word: 'world' });
    expect(wordAt('foo  bar', 4)).toBeNull();   // middle of spaces
  });

  it('rejects identifiers starting with a digit', () => {
    expect(wordAt('123abc', 3)).toBeNull();
  });

  it('returns null out-of-bounds', () => {
    expect(wordAt('foo', -1)).toBeNull();
  });
});

// ── computeIndentGuides ────────────────────────────────────────────

describe('computeIndentGuides', () => {
  it('returns [] for empty input', () => {
    expect(computeIndentGuides('')).toEqual([]);
  });

  it('returns [] for a flat unindented file', () => {
    expect(computeIndentGuides('foo\nbar\nbaz')).toEqual([]);
  });

  it('one segment for a single indent level on contiguous lines', () => {
    // for i = 1:N
    //     x = 1
    //     y = 2
    // end
    const text = 'for i = 1:N\n    x = 1\n    y = 2\nend';
    const segs = computeIndentGuides(text);
    expect(segs).toEqual([{ level: 1, startLine: 1, endLine: 2 }]);
  });

  it('nested indents produce one segment per level', () => {
    const text = [
      'function foo()',     // L=0
      '    for i = 1:N',    // L=1
      '        x = 1',      // L=2
      '        y = 2',      // L=2
      '    end',            // L=1
      'end',                // L=0
    ].join('\n');
    const segs = computeIndentGuides(text);
    expect(segs).toEqual([
      { level: 1, startLine: 1, endLine: 4 },
      { level: 2, startLine: 2, endLine: 3 },
    ]);
  });

  it('blank line inside an indented block inherits the surrounding level', () => {
    const text = 'for i = 1:N\n    a\n\n    b\nend';
    // Blank line 2 sits between level-1 neighbours → inherits L=1.
    const segs = computeIndentGuides(text);
    expect(segs).toEqual([{ level: 1, startLine: 1, endLine: 3 }]);
  });

  it('blank line between blocks does NOT extend guides across', () => {
    const text = '    a\n\nb';
    // Blank line 1: prev=L1, next=L0 → min(1,0)=0. No guide leaks.
    const segs = computeIndentGuides(text);
    expect(segs).toEqual([{ level: 1, startLine: 0, endLine: 0 }]);
  });

  it('leading blank lines get level 0 (no guide leak from below)', () => {
    const text = '\n\n    foo';
    // Line 0 + 1: prev=-1, next=L1 → min(0,1)=0.
    const segs = computeIndentGuides(text);
    expect(segs).toEqual([{ level: 1, startLine: 2, endLine: 2 }]);
  });

  it('trailing blank lines get level 0', () => {
    const text = '    foo\n\n';
    // Line 1: prev=L1, next=-1 → min(1,0)=0. No leak.
    const segs = computeIndentGuides(text);
    expect(segs).toEqual([{ level: 1, startLine: 0, endLine: 0 }]);
  });

  it('tabs count as one indent level each (4 cols)', () => {
    const text = 'a\n\tb\nc';
    const segs = computeIndentGuides(text);
    expect(segs).toEqual([{ level: 1, startLine: 1, endLine: 1 }]);
  });

  it('skips partial indents (< 4 cols → level 0)', () => {
    const text = 'a\n  b\nc';
    // 2-space leading isn't a full indent step → level 0.
    expect(computeIndentGuides(text)).toEqual([]);
  });
});

// ── constants ──────────────────────────────────────────────────────

describe('constants', () => {
  it('INDENT is 4 spaces, INDENT_LEN agrees', () => {
    expect(INDENT).toBe('    ');
    expect(INDENT_LEN).toBe(4);
  });
});
