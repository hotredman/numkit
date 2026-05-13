#!/usr/bin/env python3
# tools/cleanup_pages_deployments.py
#
# Delete old GitHub Pages deployments to clear out the soft-limit pile
# (818 deployments seems to wedge Pages re-publish for our repo).
#
# Usage:
#   export GH_TOKEN=ghp_xxxx              # PAT with `repo` (or `repo_deployment`) scope
#   python tools/cleanup_pages_deployments.py --keep 20 --dry-run
#   python tools/cleanup_pages_deployments.py --keep 20      # actually delete
#
# Flags:
#   --keep N    keep the N most recent deployments, delete the rest (default 20)
#   --dry-run   show what would be deleted, don't touch anything
#   --owner / --repo override repo (default numkit/numkit-m)

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request

API = "https://api.github.com"
DEFAULT_OWNER = "numkit"
DEFAULT_REPO  = "numkit-m"


def req(method, path, token, payload=None):
    url = path if path.startswith("http") else f"{API}{path}"
    data = json.dumps(payload).encode() if payload is not None else None
    r = urllib.request.Request(url, method=method, data=data)
    r.add_header("Authorization", f"Bearer {token}")
    r.add_header("Accept", "application/vnd.github+json")
    r.add_header("X-GitHub-Api-Version", "2022-11-28")
    if data is not None:
        r.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(r, timeout=30) as resp:
            body = resp.read()
            return resp.status, dict(resp.headers), body
    except urllib.error.HTTPError as e:
        return e.code, dict(e.headers), e.read()


def req_with_retry(method, path, token, payload=None, max_wait=600):
    """Wrap req() with auto-wait on 403 rate-limit responses."""
    while True:
        status, headers, body = req(method, path, token, payload)
        if status != 403:
            return status, headers, body
        # 403 — could be primary rate limit (X-RateLimit-Reset header) or
        # secondary (Retry-After header). Honour whichever is present.
        retry_after = headers.get("Retry-After") or headers.get("retry-after")
        reset_ts    = headers.get("X-RateLimit-Reset") or headers.get("x-ratelimit-reset")
        wait = None
        if retry_after:
            try: wait = int(retry_after) + 1
            except ValueError: pass
        if wait is None and reset_ts:
            try: wait = max(int(reset_ts) - int(time.time()) + 2, 5)
            except ValueError: pass
        if wait is None:
            wait = 60
        wait = min(wait, max_wait)
        # Heuristic: if body says "rate limit" then truly throttled; else
        # propagate the 403 (could be perms error).
        if b"rate limit" not in body.lower() and b"abuse" not in body.lower():
            return status, headers, body
        print(f"  [throttled] sleeping {wait}s before retry of {method} {path[-50:]}")
        time.sleep(wait)


def list_all_deployments(token, owner, repo):
    """Page through every deployment, newest first."""
    page = 1
    out = []
    while True:
        path = f"/repos/{owner}/{repo}/deployments?per_page=100&page={page}"
        status, _, body = req("GET", path, token)
        if status != 200:
            sys.exit(f"GET deployments failed status={status} body={body[:200]}")
        chunk = json.loads(body)
        if not chunk:
            break
        out.extend(chunk)
        page += 1
        if len(chunk) < 100:
            break
        # Be polite to the API.
        time.sleep(0.2)
    return out


def deactivate_and_delete(token, owner, repo, dep_id):
    """Required dance: set status=inactive, then DELETE."""
    # Step 1: inactive status (idempotent — repeated calls are fine).
    status, _, body = req_with_retry(
        "POST", f"/repos/{owner}/{repo}/deployments/{dep_id}/statuses",
        token, payload={"state": "inactive"})
    if status not in (201, 200):
        return f"FAIL status status={status} body={body[:120]!r}"
    # Step 2: delete the deployment record.
    status, _, body = req_with_retry(
        "DELETE", f"/repos/{owner}/{repo}/deployments/{dep_id}", token)
    if status not in (204, 200):
        return f"FAIL delete status={status} body={body[:120]!r}"
    return "OK"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--owner", default=DEFAULT_OWNER)
    ap.add_argument("--repo",  default=DEFAULT_REPO)
    ap.add_argument("--keep",  type=int, default=20,
                    help="keep this many most-recent deployments")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if not token:
        sys.exit("Set GH_TOKEN (or GITHUB_TOKEN) env var with a PAT that has "
                 "'repo' (or 'repo_deployment') scope.")

    print(f"[i] Listing deployments for {args.owner}/{args.repo} ...")
    deps = list_all_deployments(token, args.owner, args.repo)
    print(f"[i] Total deployments: {len(deps)}")
    print(f"[i] Keeping {args.keep} most-recent; deleting the rest.")
    if len(deps) <= args.keep:
        print("[i] Nothing to delete.")
        return

    # API returns newest first — keep the leading slice, delete the tail.
    keep, doomed = deps[:args.keep], deps[args.keep:]
    print(f"[i] Keep {len(keep)} (newest sha={keep[0]['sha'][:8]} created={keep[0]['created_at']})")
    print(f"[i] Delete {len(doomed)} starting from sha={doomed[0]['sha'][:8]} "
          f"({doomed[0]['created_at']}) down to sha={doomed[-1]['sha'][:8]} "
          f"({doomed[-1]['created_at']})")

    if args.dry_run:
        print("[i] --dry-run: not touching anything.")
        return

    ok = fail = 0
    for i, d in enumerate(doomed, 1):
        result = deactivate_and_delete(token, args.owner, args.repo, d["id"])
        if result == "OK":
            ok += 1
        else:
            fail += 1
            print(f"  [{i}/{len(doomed)}] id={d['id']} sha={d['sha'][:8]}: {result}")
        if i % 25 == 0:
            print(f"  [{i}/{len(doomed)}] ok={ok} fail={fail}")
        # Soft pace below the 5000 req/h ceiling. Each iteration does
        # 2 mutating requests, so 1.0s/iter = ~3600 req/h total — safe.
        time.sleep(1.0)

    print(f"[i] Done. deleted={ok} failed={fail}")


if __name__ == "__main__":
    main()
