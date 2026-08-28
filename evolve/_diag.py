import json
import sys
import traceback
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import evaluator

src = Path(__file__).parent / "policy.h"
cand = Path("/tmp/adapt.h")
cand.write_text(src.read_text(encoding="utf-8").replace(
    "#define POLICY_ADAPT_DEFAULT  0", "#define POLICY_ADAPT_DEFAULT  1"), encoding="utf-8")

stage = sys.argv[1] if len(sys.argv) > 1 else "1"
fn = {"1": evaluator.evaluate_stage1, "2": evaluator.evaluate_stage2, "3": evaluator.evaluate_stage3}[stage]
try:
    print(json.dumps(fn(str(cand)), ensure_ascii=False))
except Exception:
    traceback.print_exc()
