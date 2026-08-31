#!/usr/bin/env python3
"""Clone/update fieldtest sources from the awesome-matlab-books catalog,
then prepare a UTF-8 work copy.

  python fetch.py                     # clone missing catalog repos + prepare work
  python fetch.py --no-fetch          # prepare the work copy only
  python fetch.py --category id[,id]  # restrict cloning to category(ies)
  python fetch.py --book id[,id]      # restrict cloning to book id(s)
  python fetch.py --refresh-catalog   # re-download catalog.json first
  python fetch.py --list              # print categories + book ids, no cloning

Sources live in the external catalog
https://github.com/hotredman/awesome-matlab-books (catalog.json: companion
code repositories for MATLAB books). The catalog is cached at
corpus/catalog.json — cached copy is used as-is so runs stay reproducible
against the catalog they were fetched with; --refresh-catalog pulls a new one.

Repos clone into corpus/repos/<owner--repo> (name derived from the URL, not
from the catalog's free-form `name` field). The work copy mirrors corpus/repos
into corpus/work with every .m file transcoded to UTF-8: UTF-8 sources pass
through byte-identical; GBK/GB18030 sources are converted (many real-world
repos — especially Chinese textbook code — are GBK, and BOTH engines garble
GBK under a non-Chinese locale, so transcoding restores a fair comparison).
"""
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

HERE = Path(__file__).parent
REPOS = HERE / "corpus" / "repos"
WORK = HERE / "corpus" / "work"
CATALOG_URL = ("https://raw.githubusercontent.com/hotredman/"
               "awesome-matlab-books/main/catalog.json")
CATALOG_CACHE = HERE / "corpus" / "catalog.json"


def load_catalog(refresh=False):
    if refresh or not CATALOG_CACHE.exists():
        CATALOG_CACHE.parent.mkdir(parents=True, exist_ok=True)
        print(f"downloading catalog from {CATALOG_URL}")
        with urllib.request.urlopen(CATALOG_URL) as r, \
                open(CATALOG_CACHE, "wb") as f:
            shutil.copyfileobj(r, f)
    import json
    return json.loads(CATALOG_CACHE.read_text(encoding="utf-8"))


def select_books(catalog, categories, book_ids):
    """Filter catalog['books'] by --category / --book (union; empty = all).

    `python_companions` is a separate catalog section and is deliberately
    not fetched — its companion code is Python, not MATLAB.
    """
    books = catalog["books"]
    if categories or book_ids:
        books = [b for b in books
                 if b.get("category_id") in categories or b["id"] in book_ids]
    return books


def dest_name(url):
    """https://github.com/<owner>/<repo> -> <owner--repo> (URL is the only
    canonical field; the catalog's `name` is free-form, e.g. '2nd edition')."""
    parts = url.rstrip("/").rsplit("/", 2)[-2:]
    return "--".join(parts)


def clone_all(books):
    REPOS.mkdir(parents=True, exist_ok=True)
    for book in books:
        for repo in book["repositories"]:
            url = repo["url"]
            dest = REPOS / dest_name(url)
            if (dest / ".git").exists():
                print(f"  {dest.name}: already cloned")
                continue
            print(f"  {dest.name}: cloning {url}  [{book['id']}]")
            try:
                subprocess.run(["git", "clone", "--depth", "1", url, str(dest)],
                               check=True)
            except subprocess.CalledProcessError as e:
                # A dead/unreachable companion repo must not abort the batch.
                print(f"  {dest.name}: CLONE FAILED ({e.returncode}), skipped")


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


def list_catalog(catalog):
    meta = catalog.get("metadata", {})
    print(f"catalog compiled {meta.get('compiled_date', '?')} — "
          f"{len(catalog['books'])} books, "
          f"{sum(len(b['repositories']) for b in catalog['books'])} repos")
    by_cat = {}
    for b in catalog["books"]:
        by_cat.setdefault(b["category_id"], []).append(b)
    for cat, books in by_cat.items():
        print(f"\n{cat} ({len(books)}):")
        for b in books:
            n = len(b["repositories"])
            print(f"  {b['id']}  [{n} repo{'s' if n != 1 else ''}]")


def main():
    args = sys.argv[1:]
    refresh = "--refresh-catalog" in args
    args = [a for a in args if a != "--refresh-catalog"]
    catalog = load_catalog(refresh)

    if "--list" in args:
        list_catalog(catalog)
        return

    categories, book_ids = set(), set()
    for flag, sink in (("--category", categories), ("--book", book_ids)):
        if flag in args:
            i = args.index(flag)
            if i + 1 >= len(args):
                sys.exit(f"{flag} needs a comma-separated value")
            sink.update(a for a in args[i + 1].split(",") if a)
            args[i:i + 2] = []

    books = select_books(catalog, categories, book_ids)
    if not books:
        sys.exit("no books match the given --category/--book filters")

    if "--no-fetch" not in args:
        clone_all(books)
    prepare_work()


if __name__ == "__main__":
    main()
