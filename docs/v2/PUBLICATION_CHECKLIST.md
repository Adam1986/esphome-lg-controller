# Public fork publication checklist

- [x] Keep original project Git history and licence; add thanks to JanM321 and balazsborbely.
- [x] Merge V2 into the original `lg_controller` component name (no parallel Rev3 public component).
- [x] Run 19 C++ regression tests and Python/UI guard tests.
- [x] Compile the standard Slave example under ESPHome 2026.10.2.
- [x] Document maintainer-reported successful physical deployment of V2.0.0.
- [x] Compile the floor-standing example under ESPHome 2026.10.2. Model-specific functionality still requires validation.
- [ ] Inspect `git status --short --untracked-files=all` and confirm `esphome/secrets.yaml` is ignored.
- [ ] Inspect staged diff for personal settings, keys, firmware artefacts, log identifiers and proprietary LG firmware/source.
- [ ] Verify no directly copied LG GPL source creates unmet licensing obligations; retain inherited notices and licence.
- [ ] Keep experimental/untested features clearly marked.
- [ ] Push development branch and review a pull request **into your fork's main** before tagging a public release.
