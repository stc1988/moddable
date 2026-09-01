/*---
description: https://github.com/Moddable-OpenSource/moddable/issues/680
info: |
  A scripted exec may report a match extending past the end of the subject
  string. GetSubstitution takes $' as the substring of str from
  position + matchLength, which is empty in that case, and @@replace appends
  nothing after the replacement. Neither throws.
flags: [onlyStrict]
---*/

let evil1 = new RegExp;
evil1.exec = () => ({ 0: '1234567', length: 1, index: 0 });
assert.sameValue('abc'.replace(evil1, `$'`), "", "match longer than the subject");

let evil2 = new RegExp;
evil2.exec = () => ({ 0: 'x', length: 1, index: 3 });
assert.sameValue('abc'.replace(evil2, `$'`), "abc", "match starting at the end of the subject");
