"""TEST112: Python source-inspection checks must read UTF-8 explicitly on Windows."""
import ast
from pathlib import Path

root = Path(__file__).resolve().parents[1]
test = root / 'Tests' / 'test111_altq_parameter_focus.py'
module = ast.parse(test.read_text(encoding='utf-8'), filename=str(test))
reads = [node for node in ast.walk(module)
         if isinstance(node, ast.Call)
         and isinstance(node.func, ast.Attribute)
         and node.func.attr == 'read_text']
assert len(reads) == 3, f'Expected three source reads, found {len(reads)}'
for read in reads:
    matches = [kw.value for kw in read.keywords if kw.arg == 'encoding']
    assert len(matches) == 1 and isinstance(matches[0], ast.Constant) \
        and matches[0].value == 'utf-8', 'Locale-dependent read_text found'
print('PASS: Windows tests read C++ and workflow sources as UTF-8')
