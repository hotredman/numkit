#!/usr/bin/env python3
"""
scripts/publish_doxy.py

Generates Doxygen API documentation from NumKit and deploys/pushes it directly
to the `numkit-doxy` repository (ready for GitHub Pages).
"""

import argparse
import datetime
import os
import shutil
import subprocess
import sys

def get_project_root():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    return os.path.abspath(os.path.join(script_dir, ".."))

def find_doxy_repo(explicit_path=None, root=None):
    if explicit_path:
        p = os.path.abspath(explicit_path)
        if os.path.exists(os.path.join(p, ".git")):
            return p
        raise RuntimeError(f"Specified destination is not a git repository: {p}")

    env_path = os.environ.get("NUMKIT_DOXY_DIR")
    if env_path and os.path.exists(os.path.join(env_path, ".git")):
        return os.path.abspath(env_path)

    candidates = [
        os.path.join(root, "..", "..", "hotredman", "numkit-doxy"),
        os.path.join(root, "..", "hotredman", "numkit-doxy"),
        os.path.join(root, "..", "numkit-doxy"),
        os.path.join(root, "..", "..", "numkit-doxy"),
        r"C:\Users\User\Projects\hotredman\numkit-doxy",
    ]

    for c in candidates:
        abs_c = os.path.abspath(c)
        if os.path.exists(os.path.join(abs_c, ".git")):
            return abs_c

    return None

def run_cmd(cmd, cwd, check=True):
    print(f"[{cwd}] > {' '.join(cmd)}")
    res = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True)
    if res.stdout:
        print(res.stdout, end="")
    if res.stderr:
        print(res.stderr, end="", file=sys.stderr)
    if check and res.returncode != 0:
        raise RuntimeError(f"Command failed with exit code {res.returncode}: {' '.join(cmd)}")
    return res

def main():
    parser = argparse.ArgumentParser(description="Generate and publish Doxygen docs to numkit-doxy repo")
    parser.add_argument("--dest", help="Path to numkit-doxy git repository")
    parser.add_argument("--push", action="store_true", default=True, help="Push to remote repository (default: True)")
    parser.add_argument("--no-push", dest="push", action="store_false", help="Do not push after committing")
    parser.add_argument("--skip-build", action="store_true", help="Skip Doxygen generation and use existing build/docs/html")
    args = parser.parse_args()

    root = get_project_root()
    dest_repo = find_doxy_repo(args.dest, root)

    if not dest_repo:
        print("ERROR: Could not locate `numkit-doxy` repository.", file=sys.stderr)
        print("Please specify it using --dest <path> or set NUMKIT_DOXY_DIR environment variable.", file=sys.stderr)
        sys.exit(1)

    print(f"=== NumKit Doxygen Publisher ===")
    print(f"Source:      {root}")
    print(f"Destination: {dest_repo}")

    html_dir = os.path.join(root, "build", "docs", "html")

    if not args.skip_build:
        print("\n--- Running Doxygen ---")
        run_cmd(["doxygen", "Doxyfile"], cwd=root)

    if not os.path.exists(os.path.join(html_dir, "index.html")):
        print(f"ERROR: Doxygen output not found at {html_dir}", file=sys.stderr)
        sys.exit(1)

    print("\n--- Cleaning and updating destination repository ---")
    for item in os.listdir(dest_repo):
        if item == ".git":
            continue
        item_path = os.path.join(dest_repo, item)
        if os.path.isdir(item_path):
            shutil.rmtree(item_path)
        else:
            os.remove(item_path)

    for item in os.listdir(html_dir):
        src_item = os.path.join(html_dir, item)
        dst_item = os.path.join(dest_repo, item)
        if os.path.isdir(src_item):
            shutil.copytree(src_item, dst_item)
        else:
            shutil.copy2(src_item, dst_item)

    # Ensure .nojekyll exists for GitHub Pages
    nojekyll_path = os.path.join(dest_repo, ".nojekyll")
    with open(nojekyll_path, "w", encoding="utf-8") as f:
        f.write("# Disable Jekyll for GitHub Pages\n")

    # Get current numkit commit hash
    git_hash = "unknown"
    try:
        res = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=root, capture_output=True, text=True)
        if res.returncode == 0:
            git_hash = res.stdout.strip()
    except Exception:
        pass

    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    commit_msg = f"docs: update Doxygen API documentation from numkit {git_hash} ({timestamp})"

    print("\n--- Staging and committing changes ---")
    run_cmd(["git", "add", "-A"], cwd=dest_repo)

    status_res = run_cmd(["git", "status", "--porcelain"], cwd=dest_repo, check=False)
    if not status_res.stdout.strip():
        print("No changes to commit in destination repository.")
    else:
        run_cmd(["git", "commit", "-m", commit_msg], cwd=dest_repo)
        print(f"Committed: {commit_msg}")

    if args.push:
        print("\n--- Pushing to remote (origin main) ---")
        run_cmd(["git", "push", "origin", "main"], cwd=dest_repo)
        print("\n>>> Successfully deployed and pushed Doxygen docs to GitHub Pages repo!")
    else:
        print("\n>>> Documentation generated and committed locally. (Use --push to push to GitHub).")

if __name__ == "__main__":
    main()
