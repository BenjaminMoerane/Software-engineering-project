#!/usr/bin/env python3
"""Checks that every test ID named in docs/test/RTM.csv exists in the test sources
and that every test source ID is traced to at least one requirement.
Usage: check_rtm.py <repo-root>"""
import csv, re, sys, pathlib

root = pathlib.Path(sys.argv[1])
ids_in_tests = set()
for p in (root / "tests" / "unit").glob("*.cpp"):
    ids_in_tests |= set(re.findall(r"\b(T_[A-Z_]+_\d+)_", p.read_text()))
st = (root / "tests" / "system" / "run_system_tests.py").read_text()
sys_ids = set(re.findall(r"def test_(ST_[A-Z0-9]+_\d+)_", st))
traced = set()
bad = []
with open(root / "docs" / "test" / "RTM.csv", newline="") as f:
    for row in csv.DictReader(f):
        for col in ("Unit and integration tests", "System and acceptance tests"):
            for tid in row[col].split():
                if not re.fullmatch(r"(T_[A-Z_]+_\d+|ST_[A-Z0-9]+_\d+)", tid):
                    continue  # words such as "ctest"
                traced.add(tid)
                if tid not in ids_in_tests and tid not in sys_ids:
                    bad.append((row["Requirement"], tid))
untraced = sorted((ids_in_tests | sys_ids) - traced)
print(f"unit/integration test IDs: {len(ids_in_tests)}; system test IDs: {len(sys_ids)}; traced: {len(traced)}")
print("IDs named in the RTM that do not exist:", bad or "none")
print("test IDs not traced to any requirement:", untraced or "none")
sys.exit(1 if bad else 0)
