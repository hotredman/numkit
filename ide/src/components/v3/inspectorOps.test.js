import { describe, it, expect } from 'vitest';
import { pathToMatlabLValue, valueToMatlabRHS, isValidIdentifier } from './inspectorOps';

describe('pathToMatlabLValue', () => {
  it('root with empty path → just the name', () => {
    expect(pathToMatlabLValue('car', '')).toBe('car');
    expect(pathToMatlabLValue('car', undefined)).toBe('car');
  });
  it('field steps → dotted', () => {
    expect(pathToMatlabLValue('car', 'f:engine;f:hp')).toBe('car.engine.hp');
  });
  it('struct-array element → 1-based ()', () => {
    expect(pathToMatlabLValue('s', 'e:2;f:data')).toBe('s(3).data');
  });
  it('cell index → 1-based {}', () => {
    expect(pathToMatlabLValue('c', 'c:3')).toBe('c{4}');
  });
  it('mixed chain', () => {
    expect(pathToMatlabLValue('x', 'f:a;e:0;c:1;f:b')).toBe('x.a(1){2}.b');
  });
  it('ignores empty / malformed tokens', () => {
    expect(pathToMatlabLValue('x', 'f:a;;bad')).toBe('x.a');
  });
});

describe('valueToMatlabRHS', () => {
  it('numeric → bare number + value', () => {
    expect(valueToMatlabRHS('3.14', 'double')).toEqual({ rhs: '3.14', value: 3.14 });
    expect(valueToMatlabRHS('-5', 'int32')).toEqual({ rhs: '-5', value: -5 });
  });
  it('numeric rejects non-finite', () => {
    expect(valueToMatlabRHS('abc', 'double')).toBeNull();
    expect(valueToMatlabRHS('', 'double')).toBeNull();
    expect(valueToMatlabRHS('Inf', 'double')).toBeNull();
  });
  it('logical parses true/false/1/0', () => {
    expect(valueToMatlabRHS('true', 'logical')).toEqual({ rhs: 'true', value: true });
    expect(valueToMatlabRHS('0', 'logical')).toEqual({ rhs: 'false', value: false });
    expect(valueToMatlabRHS('FALSE', 'logical')).toEqual({ rhs: 'false', value: false });
    expect(valueToMatlabRHS('maybe', 'logical')).toBeNull();
  });
  it('char single-quotes and escapes apostrophes', () => {
    expect(valueToMatlabRHS('x', 'char')).toEqual({ rhs: "'x'", value: 'x' });
    expect(valueToMatlabRHS("it's", 'char')).toEqual({ rhs: "'it''s'", value: "it's" });
  });
  it('string double-quotes and escapes double-quotes', () => {
    expect(valueToMatlabRHS('hi', 'string')).toEqual({ rhs: '"hi"', value: 'hi' });
    expect(valueToMatlabRHS('a"b', 'string')).toEqual({ rhs: '"a""b"', value: 'a"b' });
  });
  it('complex editing deferred → null', () => {
    expect(valueToMatlabRHS('1+2i', 'complex')).toBeNull();
  });
  it('escaping blocks injection in char', () => {
    // A malicious value can't terminate the literal — quotes are doubled.
    const r = valueToMatlabRHS("'); evil(", 'char');
    expect(r.rhs).toBe("'''); evil('");
  });
});

describe('isValidIdentifier', () => {
  it('accepts valid identifiers', () => {
    expect(isValidIdentifier('foo')).toBe(true);
    expect(isValidIdentifier('_x1')).toBe(true);
    expect(isValidIdentifier('Field_2')).toBe(true);
  });
  it('rejects invalid', () => {
    expect(isValidIdentifier('1foo')).toBe(false);
    expect(isValidIdentifier('a-b')).toBe(false);
    expect(isValidIdentifier('a b')).toBe(false);
    expect(isValidIdentifier('')).toBe(false);
    expect(isValidIdentifier(null)).toBe(false);
    expect(isValidIdentifier('a.b')).toBe(false);
  });
});
