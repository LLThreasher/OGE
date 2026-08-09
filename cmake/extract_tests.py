import re
import sys

assert len(sys.argv) > 1, "Usage: extract_tests.py <source.cpp> [source2.cpp ...]"

test_names = []
for filepath in sys.argv[1:]:
    with open(filepath, "r") as f:
        # Match TEST(name) — name captures the test identifier.
        found = re.findall(r"TEST\((\w+)\)", f.read())
        test_names.extend(found)

# Output semicolon-separated list for CMake's list parser.
print(";".join(test_names))
