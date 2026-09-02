#!/usr/bin/env python3
"""deploy_pages.py — unified cross-platform publisher for NumKit GitHub Pages repositories."""

import os
import sys
import shutil
import argparse
import subprocess
from pathlib import Path

if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except AttributeError:
        pass

ROOT = Path(__file__).resolve().parent.parent

def run_cmd(args, cwd=ROOT, check=True):
    res = subprocess.run(args, cwd=str(cwd), capture_output=True, text=True, encoding='utf-8', errors='replace')
    if check and res.returncode != 0:
        print(f"ERROR executing: {' '.join(args)} (cwd={cwd})", file=sys.stderr)
        if res.stdout:
            print("STDOUT:", res.stdout, file=sys.stderr)
        if res.stderr:
            print("STDERR:", res.stderr, file=sys.stderr)
        sys.exit(res.returncode)
    return res

def get_git_rev():
    res = run_cmd(['git', 'rev-parse', '--short', 'HEAD'], check=False)
    if res.returncode == 0 and res.stdout.strip():
        return res.stdout.strip()
    return 'manual'

def deploy(target_type, source_dir, repo_url, dest_dir=None, push=True, skip_build=False):
    source_dir = Path(source_dir).resolve()
    print(f"=== NumKit Deploy to GitHub Pages [{target_type.upper()}] ===")
    print(f"Source Directory: {source_dir}")

    # 1. Build if needed
    if not skip_build:
        if target_type == 'bugs':
            print("Building NumKit Bugs Docsify site...")
            run_cmd([sys.executable, str(ROOT / 'tools' / 'build_bugs_site.py'), '--output', str(source_dir)])
        elif target_type == 'doxy':
            print("Building Doxygen documentation...")
            run_cmd(['doxygen', 'Doxyfile'], cwd=ROOT)
        elif target_type == 'demo':
            print("Building Web IDE bundle...")
            if sys.platform == 'win32':
                run_cmd([str(ROOT / 'scripts' / 'web-build.bat')], cwd=ROOT)
            else:
                run_cmd([str(ROOT / 'scripts' / 'web-build.sh')], cwd=ROOT)

    if not (source_dir / 'index.html').exists():
        print(f"ERROR: {source_dir / 'index.html'} not found after build!", file=sys.stderr)
        sys.exit(1)

    src_rev = get_git_rev()

    # 2. Determine target workspace
    if dest_dir:
        deploy_dir = Path(dest_dir).resolve()
        is_custom_dest = True
    else:
        deploy_dir = ROOT / 'build' / f'deploy-{target_type}'
        is_custom_dest = False

    deploy_dir.mkdir(parents=True, exist_ok=True)
    print(f"Deploy Workspace: {deploy_dir}")
    if not is_custom_dest:
        print(f"Target Remote:   {repo_url}")

    # 3. Initialize or configure git repository in deploy workspace
    if not (deploy_dir / '.git').exists():
        run_cmd(['git', 'init', '-b', 'main'], cwd=deploy_dir)
        if not is_custom_dest:
            run_cmd(['git', 'remote', 'add', 'origin', repo_url], cwd=deploy_dir)
    else:
        if not is_custom_dest:
            run_cmd(['git', 'remote', 'set-url', 'origin', repo_url], cwd=deploy_dir, check=False)

    # 4. Clean old files in deploy workspace (preserving .git)
    print("Syncing files to deploy workspace...")
    for item in deploy_dir.iterdir():
        if item.name == '.git':
            continue
        if item.is_dir():
            shutil.rmtree(item)
        else:
            item.unlink()

    # Copy fresh artifacts
    for item in source_dir.iterdir():
        dest_item = deploy_dir / item.name
        if item.is_dir():
            shutil.copytree(item, dest_item)
        else:
            shutil.copy2(item, dest_item)

    # Ensure .nojekyll exists
    (deploy_dir / '.nojekyll').touch()
    print("Files synchronized.")

    # 5. Commit 1 clean state (Orphan Branch)
    print("Creating clean 1-commit state...")
    msg_map = {
        'bugs': f"docs(bugs): NumKit Defect & Parity Catalog (numkit@{src_rev})",
        'doxy': f"docs(doxy): NumKit C++ API Documentation (numkit@{src_rev})",
        'demo': f"deploy(demo): NumKit Web IDE Demo (numkit@{src_rev})"
    }
    commit_msg = msg_map.get(target_type, f"deploy: NumKit {target_type} (numkit@{src_rev})")

    run_cmd(['git', 'checkout', '--orphan', 'temp_deploy'], cwd=deploy_dir, check=False)
    run_cmd(['git', 'add', '-A'], cwd=deploy_dir)
    run_cmd(['git', 'commit', '-m', commit_msg], cwd=deploy_dir)
    run_cmd(['git', 'branch', '-D', 'main'], cwd=deploy_dir, check=False)
    run_cmd(['git', 'branch', '-m', 'main'], cwd=deploy_dir)

    # 6. Push
    if push and not is_custom_dest:
        print(f"Force-pushing clean 1-commit state to {repo_url}...")
        push_res = run_cmd(['git', 'push', '-f', 'origin', 'main'], cwd=deploy_dir, check=False)
        if push_res.returncode != 0:
            print(f"ERROR: git push failed!\n{push_res.stderr}", file=sys.stderr)
            sys.exit(1)
        print(f"\n[SUCCESS] Published clean 1-commit mirror to GitHub Pages: {repo_url}")
    else:
        print(f"\n[OK] Clean commit created locally in: {deploy_dir} (push skipped).")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Deploy NumKit docs/demo to GitHub Pages repository.')
    parser.add_argument('--type', choices=['bugs', 'doxy', 'demo'], default='bugs', help='Type of deployment')
    parser.add_argument('--source', help='Source directory of built site')
    parser.add_argument('--repo', help='Remote Git repository URL')
    parser.add_argument('--dest', help='Optional local destination directory')
    parser.add_argument('--no-push', action='store_true', help='Skip pushing to remote')
    parser.add_argument('--skip-build', action='store_true', help='Skip building site')

    args = parser.parse_args()

    default_sources = {
        'bugs': ROOT / 'build' / 'bugs',
        'doxy': ROOT / 'build' / 'docs' / 'html',
        'demo': ROOT / 'deploy'
    }
    default_repos = {
        'bugs': os.environ.get('NUMKIT_BUGS_REPO', 'git@github.com:hotredman/numkit-bugs.git'),
        'doxy': os.environ.get('NUMKIT_DOXY_REPO', 'git@github.com:hotredman/numkit-doxy.git'),
        'demo': os.environ.get('NUMKIT_DEMO_REPO', 'git@github.com:hotredman/numkit-demo.git')
    }

    src = args.source or default_sources[args.type]
    repo = args.repo or default_repos[args.type]

    deploy(
        target_type=args.type,
        source_dir=src,
        repo_url=repo,
        dest_dir=args.dest,
        push=not args.no_push,
        skip_build=args.skip_build
    )
