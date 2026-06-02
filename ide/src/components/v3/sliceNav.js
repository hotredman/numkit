// sliceNav.js — page navigation math for 3-D / N-D array viewing.
//
// A "page" is one 2-D rows×cols slice of an array. In numkit (column-major)
// the slices of a 3-D / N-D array are laid out contiguously, ordered
// column-major over dimensions 3..N, so a single linear page index p in
// [0, pageCount) addresses any slice — for any rank. dims is the full shape
// [d0, d1, d2, …]; only dims[2..] participate in paging (d0=rows, d1=cols).
//
// The mapping mirrors MATLAB A(:, :, k3, k4, …): subs[0] is the 0-based
// subscript for dim 3, subs[1] for dim 4, and so on.

/** Number of 2-D slices = product of dims[2..] (1 for a 2-D array). */
export function pageCount(dims) {
  if (!Array.isArray(dims)) return 1;
  let n = 1;
  for (let i = 2; i < dims.length; i++) n *= (dims[i] || 1);
  return n;
}

/** Linear page index → 0-based subscripts for dims[2..] (column-major). */
export function pageToSubs(page, dims) {
  const subs = [];
  if (!Array.isArray(dims)) return subs;
  let rem = Math.max(0, page | 0);
  for (let i = 2; i < dims.length; i++) {
    const d = dims[i] || 1;
    subs.push(rem % d);
    rem = Math.floor(rem / d);
  }
  return subs;
}

/** 0-based subscripts for dims[2..] → linear page index (column-major). */
export function subsToPage(subs, dims) {
  if (!Array.isArray(dims)) return 0;
  let page = 0;
  let stride = 1;
  for (let i = 2; i < dims.length; i++) {
    const d = dims[i] || 1;
    const k = Math.min(d - 1, Math.max(0, (subs[i - 2] || 0) | 0));
    page += k * stride;
    stride *= d;
  }
  return page;
}
