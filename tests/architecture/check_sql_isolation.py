#!/usr/bin/env python3
"""Architecture rule (brief Phase 4: "SQLite ... accessed only through a data-access layer").

Fails if any application source outside src/data/ contains SQL text, includes <sqlite3.h>, calls the SQLite C API,
or uses the low-level data::Statement / data::Transaction / Database::handle() primitives.
Run by CTest (lms_architecture_sql_isolation) and therefore by CI on every push.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
SRC = ROOT / "src"
ALLOWED = SRC / "data"

RULES = [
    (re.compile(r'"[^"\n]*\b(SELECT|INSERT\s+INTO|UPDATE\s+\w+\s+SET|DELETE\s+FROM|CREATE\s+TABLE|PRAGMA)\b', re.I),
     "SQL text"),
    (re.compile(r"#\s*include\s*<sqlite3\.h>"), "includes sqlite3.h"),
    (re.compile(r"\bsqlite3_\w+\s*\("), "calls the SQLite C API"),
    (re.compile(r"\b(data::)?Statement\s+\w+\s*\("), "uses data::Statement"),
    (re.compile(r"\b(data::)?Transaction\s+\w+\s*\("), "uses data::Transaction"),
    (re.compile(r"\.handle\(\)"), "uses Database::handle()"),
]


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def main() -> int:
    violations = []
    files = sorted(p for p in SRC.rglob("*") if p.suffix in {".cpp", ".hpp", ".h"})
    for path in files:
        if ALLOWED in path.parents:
            continue
        code = strip_comments(path.read_text(encoding="utf-8"))
        for lineno, line in enumerate(code.splitlines(), 1):
            for pattern, label in RULES:
                if pattern.search(line):
                    violations.append(f"{path.relative_to(ROOT)}:{lineno}: {label}: {line.strip()}")
    checked = sum(1 for p in files if ALLOWED not in p.parents)
    if violations:
        print("SQL-isolation rule FAILED:")
        print("\n".join(violations))
        return 1
    print(f"SQL-isolation rule passed: {checked} source files outside src/data/ contain no SQL or SQLite access.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
