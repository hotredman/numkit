#!/usr/bin/env python3
"""Clone/update fieldtest sources, then prepare a UTF-8 work copy.

  python fetch.py            # clone missing repos + (re)prepare corpus/work
  python fetch.py --no-fetch # prepare the work copy only

The work copy mirrors corpus/repos into corpus/work with every .m file
transcoded to UTF-8: UTF-8 sources pass through byte-identical; GBK/GB18030
sources are converted (many real-world repos — especially Chinese textbook
code — are GBK, and BOTH engines garble GBK under a non-Chinese locale, so
transcoding restores a fair comparison).
"""
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).parent
REPOS = HERE / "corpus" / "repos"
WORK = HERE / "corpus" / "work"


def load_sources():
    out = []
    for line in (HERE / "sources.list").read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        url, lic, note = (part.strip() for part in line.split("|"))
        out.append((url, lic, note))
    return out


def clone_all():
    REPOS.mkdir(parents=True, exist_ok=True)
    for url, _lic, _note in load_sources():
        name = url.rsplit("/", 1)[-1].removesuffix(".git")
        dest = REPOS / name
        if (dest / ".git").exists():
            print(f"  {name}: already cloned")
        else:
            print(f"  {name}: cloning {url}")
            subprocess.run(["git", "clone", "--depth", "1", url, str(dest)], check=True)


def transcode_to_utf8(src_bytes: bytes) -> tuple[bytes, str]:
    """Return (utf8_bytes, encoding_used). Tries UTF-8 first, then GB18030."""
    try:
        src_bytes.decode("utf-8")
        return src_bytes, "utf-8"
    except UnicodeDecodeError:
        pass
    try:
        text = src_bytes.decode("gb18030")
        return text.encode("utf-8"), "gb18030"
    except UnicodeDecodeError:
        return src_bytes, "undecodable (left as-is)"


def prepare_work():
    n_conv = 0
    for repo in sorted(REPOS.iterdir()):
        if not repo.is_dir():
            continue
        dest = WORK / repo.name
        if dest.exists():
            shutil.rmtree(dest)
        dest.mkdir(parents=True)
        for f in repo.rglob("*"):
            if not f.is_file():
                continue
            # skip git metadata and large binaries; keep everything else
            rel = f.relative_to(repo)
            if any(part in (".git",) for part in rel.parts):
                continue
            target = dest / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            if f.suffix.lower() == ".m":
                data, enc = transcode_to_utf8(f.read_bytes())
                if enc == "gb18030":
                    n_conv += 1
                target.write_bytes(data)
            else:
                shutil.copy2(f, target)
        print(f"  prepared {repo.name} -> corpus/work/{repo.name}")
    print(f"transcoded {n_conv} GBK .m files to UTF-8")


if __name__ == "__main__":
    if "--no-fetch" not in sys.argv:
        clone_all()
    prepare_work()
