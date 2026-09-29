"""Layer enforcement for the Soteria include graph (no toolchain required).

Soteria includes are either <system> or quoted repo-relative paths, so the
dependency graph can be built by scanning #include directives directly —
no clang-scan-deps, no compile database.  (GameBits needs clang-scan-deps
because of its scale; Soteria's 20-odd TUs do not.)

Rules (docs/ARCHITECTURE.md — Layer rules):

  engine/core      -> engine only; must NOT include rendering, wsi, data, app
  engine/data      -> core interfaces only; must NOT include rendering, wsi, app
  engine/rendering -> may include core + data; must NOT include wsi, app
  engine/wsi       -> may include core; must NOT include rendering, data, app
  app, tests       -> may include engine anywhere; engine must never include them

Exit 0 = clean, 1 = violations, 77 = python/env problem (ctest SKIP).
Run:  python tools/check_layers.py
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)

SCAN_DIRS = ("engine", "app", "tests")
SOURCE_EXTS = (".hpp", ".cpp", ".h", ".cxx", ".cc")


def block_of(relpath):
    """Map a repo-relative path to its architectural block."""
    p = relpath.replace("\\", "/")
    if p.startswith("engine/core/"):
        return "core"
    if p.startswith("engine/data/"):
        return "data"
    if p.startswith("engine/rendering/"):
        return "rendering"
    if p.startswith("engine/wsi/"):
        return "wsi"
    if p.startswith("engine/"):
        return "engine"          # engine root headers, if any
    if p.startswith("app/"):
        return "app"
    if p.startswith("tests/"):
        return "tests"
    return None


# block -> set of blocks it may NOT include from
FORBIDDEN = {
    "core":      {"rendering", "wsi", "data", "app", "tests"},
    "data":      {"rendering", "wsi", "app", "tests"},
    "rendering": {"wsi", "app", "tests"},
    "wsi":       {"rendering", "data", "app", "tests"},
    "engine":    {"app", "tests"},
}


def resolve(includer, inc):
    """Resolve a quoted include relative to the including file, then the repo."""
    cand = os.path.normpath(os.path.join(os.path.dirname(includer), inc))
    if os.path.isfile(cand):
        return os.path.relpath(cand, REPO)
    cand = os.path.normpath(os.path.join(REPO, inc))
    if os.path.isfile(cand):
        return os.path.relpath(cand, REPO)
    return None


def main():
    violations = []
    scanned = 0
    for scan in SCAN_DIRS:
        root = os.path.join(REPO, scan)
        for dirpath, _dirs, files in os.walk(root):
            for name in files:
                if not name.lower().endswith(SOURCE_EXTS):
                    continue
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, REPO)
                scanned += 1
                with open(full, encoding="utf-8", errors="replace") as f:
                    text = f.read()
                includer_block = block_of(rel)
                if includer_block not in FORBIDDEN:
                    continue
                for inc in INCLUDE_RE.findall(text):
                    target = resolve(full, inc)
                    if target is None:
                        continue  # generated or SDK header — not our layer concern
                    target_block = block_of(target)
                    if target_block in FORBIDDEN[includer_block]:
                        violations.append(
                            f"{rel} -> {target} "
                            f"({includer_block} may not include {target_block})")
    if violations:
        print("LAYER VIOLATIONS:")
        for v in sorted(violations):
            print("  " + v)
        return 1
    print(f"layers clean ({scanned} source files scanned)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
