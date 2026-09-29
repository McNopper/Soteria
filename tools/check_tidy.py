"""clang-tidy over the compilation database — Soteria execution lane.

The .clang-tidy profile is config-only; this driver is the execution lane
(pattern adapted from GameBits tools/check_tidy.py).  It runs clang-tidy
(VS LLVM component, resolved via vswhere) on every engine/ translation unit
listed in the compile database in parallel.

Scope: engine/ sources ONLY — the certifiable layer.  app/ is simulation
code and tests/ use GTest; neither is analysed by the MISRA profile (see
.clang-tidy header comment).

Unlike GameBits, Soteria's profile treats EVERY finding as an error
(WarningsAsErrors: '*'), so any diagnostic line fails the gate.

Exit 0 = clean, 1 = findings, 77 = toolchain/DB missing (ctest SKIP).
Run:  python tools/check_tidy.py [--db PATH] [--jobs N]
"""

import concurrent.futures
import json
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VSWHERE = r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
TIDY_FALLBACK = r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-tidy.exe"
DEFAULT_DB = os.path.join(REPO, "build", "compile_commands.json")

DIAG = re.compile(r"^(.+):(\d+):(\d+): (?:warning|error): (.+) \[([a-zA-Z0-9_.-]+)\]$",
                  re.MULTILINE)


def find_clang_tidy():
    """Resolve clang-tidy from the newest VS install (vswhere), so a Visual
    Studio upgrade or edition change needs no repo edit."""
    try:
        root = subprocess.check_output(
            [VSWHERE, "-latest", "-property", "installationPath"], text=True
        ).strip()
        p = os.path.join(root, r"VC\Tools\Llvm\x64\bin\clang-tidy.exe")
        if os.path.isfile(p):
            return p
    except (OSError, subprocess.CalledProcessError):
        pass
    return TIDY_FALLBACK


def is_engine_tu(path):
    p = path.replace("\\", "/").lower()
    if "/build/" in p or "/_deps/" in p:
        return False
    return "/engine/" in p


def sanitize_db(db_path, tmp_dir):
    # Strip module-scanning / depfile flags that reference files existing
    # only after a build, so the run works on a fresh configure.
    entries = json.load(open(db_path))
    out = []
    for e in entries:
        if not is_engine_tu(e["file"]):
            continue
        parts = e["command"].split()
        parts = [p for p in parts
                 if not p.startswith("@CMakeFiles")
                 and p not in ("-fmodules-ts", "-MD")
                 and not p.startswith("-fmodule-mapper")
                 and not p.startswith("-fdeps-format")]
        e = dict(e)
        e["command"] = " ".join(parts)
        out.append(e)
    db = os.path.join(tmp_dir, "compile_commands.json")
    with open(db, "w") as f:
        json.dump(out, f)
    return out


def run_tu(tidy, tmp_dir, path):
    r = subprocess.run([tidy, "--quiet", "-p", tmp_dir, path,
                        "--extra-arg=--driver-mode=cl"],
                       capture_output=True, text=True)
    return path, r.stdout + r.stderr


def main():
    db = DEFAULT_DB
    jobs = max(4, (os.cpu_count() or 4) - 2)
    args = sys.argv[1:]
    while args:
        a = args.pop(0)
        if a == "--db" and args:
            db = args.pop(0)
        elif a == "--jobs" and args:
            jobs = int(args.pop(0))
    tidy = find_clang_tidy()
    if not os.path.isfile(tidy):
        print("clang-tidy not found: " + tidy)
        return 77
    if not os.path.isfile(db):
        print("compile_commands.json not found: " + db + " (configure the build first)")
        return 77
    with tempfile.TemporaryDirectory(prefix="soteria_tidy_") as tmp_dir:
        entries = sanitize_db(db, tmp_dir)
        files = [e["file"] for e in entries]
        if not files:
            print("no engine translation units in compile database")
            return 77
        outputs = {}
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
            futs = {pool.submit(run_tu, tidy, tmp_dir, f): f for f in files}
            for fut in concurrent.futures.as_completed(futs):
                path, text = fut.result()
                outputs[path] = text
    log_path = os.path.join(os.path.dirname(db), "clang_tidy.log") \
        if os.path.dirname(db) else os.path.join(REPO, "clang_tidy.log")
    with open(log_path, "w") as f:
        for path in sorted(outputs):
            f.write(f"===== {path}\n{outputs[path]}\n")
    findings = []
    for path, text in outputs.items():
        for m in DIAG.finditer(text):
            file, line, col, msg, check = m.groups()
            findings.append(f"{file}:{line}:{col}: {msg} [{check}]")
    if findings:
        print(f"CLANG-TIDY FINDINGS ({len(findings)}; WarningsAsErrors: '*' — all fail):")
        for e in findings:
            print("  " + e)
        print(f"\nfull log: {log_path}")
        return 1
    print(f"clang-tidy clean ({len(files)} engine translation units scanned; log: {log_path})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
