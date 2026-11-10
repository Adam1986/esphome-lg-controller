"""Regression checks for backwards-compatible Floor Console YAML option names.

Loads only the normalization function and constant from the AST so this
works without an ESPHome installation. Full schema verification is done by
`esphome config` in Codespaces.
"""
from pathlib import Path
import ast

source = (Path(__file__).resolve().parents[1] / "components/lg_controller/climate.py").read_text()
tree = ast.parse(source)
items = [node for node in tree.body if (
    isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "LEGACY_FLOOR_CONSOLE_KEYS" for t in node.targets)
) or (isinstance(node, ast.FunctionDef) and node.name == "_normalize_floor_console_keys")]
assert len(items) == 2
class CV:
    class Invalid(Exception):
        pass
ns = {"cv": CV}
exec(compile(ast.Module(body=items, type_ignores=[]), "<aliases>", "exec"), ns)
legacy = ns["LEGACY_FLOOR_CONSOLE_KEYS"]
normalize = ns["_normalize_floor_console_keys"]
assert len(legacy) == 14
for old, new in legacy.items():
    assert new.startswith("floor_console_")
    value = {"name": "example"} if old.endswith(("control", "status", "master", "indoor", "capability")) else True
    actual = normalize({old: value, "is_slave_controller": True})
    assert old not in actual
    assert actual[new] == value
    assert actual["is_slave_controller"] is True
    assert normalize({new: value}) == {new: value}
    try:
        normalize({old: value, new: value})
    except CV.Invalid:
        pass
    else:
        raise AssertionError(f"Conflicting old and new keys not rejected: {old}")
assert normalize(None) is None
print("PASS: 14 legacy Floor Console keys map to their canonical names")
