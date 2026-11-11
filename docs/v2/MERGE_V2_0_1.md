# V2.0.1 Floor Console template recovery — merge instructions

This patch is based **exactly** on the uploaded source export of
`feature/floor-console-naming` (export commit `a1efd8ab8b94847e158411c7a7b6561f46c6b24f`).
It does not include a live GitHub push or merge. The supplied `.patch` is
formatted for `git am` and contains **only the differences** to that export.

## Incorporate into the existing naming branch

In a clean clone of the repository containing `feature/floor-console-naming`:

```bash
git fetch origin
git switch feature/floor-console-naming
git pull --ff-only
# Ideally verify HEAD is the ZIP's source commit before applying.
git switch -c fix/floor-console-template-recovery
git am /path/to/LG-Controller-V2.0.1-Floor-Console-Template-Recovery.patch
bash scripts/run-v2-offline-tests.sh
esphome config esphome/lg-example-standard.yaml
esphome config esphome/lg-example-floor-console.yaml
esphome compile esphome/lg-example-standard.yaml
esphome compile esphome/lg-example-floor-console.yaml
git push -u origin fix/floor-console-template-recovery
```

Create a pull request **into `feature/floor-console-naming`** (or merge that
branch locally after compiling and testing). When the combined branch is
satisfactory, merge the combined naming + reliability branch into `main`
using your normal PR/CI process. Do not merge blindly before physical testing.

If `git am` reports changed context or conflicts, do not force the patch;
review the differences against the updated branch and reconcile them.

## What changed

- No new user-facing YAML options; no change to the canonical
  `floor_console_*` names or the legacy `floorstanding_*` aliases.
- For a Floor Console command missing a genuine page template, bounded
  read-only `28` full-settings discovery precedes the optional page write.
- Keeps the current Floor Console no-active-sleep guard, genuine A8+C8
  state matching, bounded retry/readback rules and physical Master authority.
- `LgSelect` / `LgSwitch` request handlers for floor console controls no
  longer publish a requested value as confirmed. Only genuine observed
  readback can change the Home Assistant control state.

## Hardware check — Garden Office only

1. Confirm V2.0.1 on startup; verify existing climate, sleep and pipe poll.
2. Reboot the ESP to clear page templates. Once a genuine A8+C8 pair arrives,
   request Top. Expect `FLOOR TEMPLATE: guarded 28`, then a genuine CD,
   then `FLOOR TX: page 5`, then two matching indoor CD reports.
3. Only then should Home Assistant display Top as confirmed. Check physical
   outlet selection as well.
4. Repeat for Bottom and Both, one request at a time.
5. If the read cannot safely proceed, expect timeout and **no 2D command**.
   In this case HA remains at its last confirmed selection.

This is hardware-experimental functionality. Offline tests cover pure wire
building, scheduling and UI request-handling, not ESPHome firmware compilation,
controller readback scheduling or the physical discharge mechanism.
