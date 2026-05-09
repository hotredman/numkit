# Test fixtures

`sample-folder/` — a small project-shaped directory used by the
local-folder e2e tests. It deliberately includes a `node_modules/`
subdir so `local-folder.spec.js` can verify that
`TREE_SKIP_DIRS` (in `ide/desktop/main.js`) is still filtering it
out of the Sidebar tree. If you add new dirs to that skip list,
mirror them here so they're regression-tested too.

The fixture must NOT be `.gitignore`d — tests check it in directly
to keep "git clone → npm test" working without extra setup.
