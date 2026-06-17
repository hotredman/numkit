clear
import compat.*

% ── dictionary (value semantics, R2022b+) ────────────────────
d = dictionary(["a" "b" "c"], [10 20 30]);
fprintf('class(d) = %s        (expect dictionary)\n', class(d));
fprintf('d("b") = %g          (expect 20)\n', d("b"));
fprintf('numEntries = %d      (expect 3)\n', numEntries(d));
d("d") = 40;
fprintf('after insert = %d    (expect 4)\n', numEntries(d));
fprintf('isKey a/z = %d %d    (expect 1 0)\n', isKey(d, "a"), isKey(d, "z"));
d2 = d; d2("a") = 99;
fprintf('VALUE sem d("a") = %g (expect 10 — copy is independent)\n', d("a"));
dn = dictionary([1 2 3], [100 200 300]);
fprintf('numeric key dn(2) = %g (expect 200)\n', dn(2));
disp(d)

% ── containers.Map (handle semantics, legacy) ────────────────
m = containers.Map({'one', 'two'}, {1, 2});
fprintf('class(m) = %s        (expect containers.Map)\n', class(m));
fprintf('m(''one'') = %g       (expect 1)\n', m('one'));
fprintf('m.Count = %d         (expect 2)\n', m.Count);
m2 = m; m2('one') = 99;
fprintf('HANDLE sem m(''one'') = %g (expect 99 — alias shares state)\n', m('one'));
fprintf('isKey two = %d       (expect 1)\n', isKey(m, 'two'));
remove(m, 'two');
fprintf('after remove Count = %d (expect 1)\n', m.Count);
disp(m)
