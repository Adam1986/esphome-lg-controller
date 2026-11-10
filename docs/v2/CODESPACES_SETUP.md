# Develop this V2 fork in GitHub Codespaces

This repository is a **direct GitHub fork of JanM321/esphome-lg-controller**. The V2 implementation is integrated under the original public component name, `lg_controller`; no separately named ESPHome component is required.

1. Open your fork in GitHub and start a Codespace from `main`.
2. Create a branch before making changes: `git switch -c feature/lg-controller-v2` (or a newer feature branch).
3. The enhanced C++/Python component is in `esphome/components/lg_controller/`; supporting base YAMLs, documentation and tests are in `esphome/`, `docs/v2/` and `scripts/`.
4. Set up an isolated Python environment and install a supported ESPHome version. V2.0.0 was compiled with `esphome==2026.10.2`.
5. Place private credentials in `esphome/secrets.yaml` (ignored by Git). Do **not** commit actual or test credentials.
6. Run:

```bash
bash scripts/run-v2-offline-tests.sh
esphome config esphome/lg-example-standard.yaml
esphome compile esphome/lg-example-standard.yaml
```

7. Optionally validate the floor console example with `esphome config esphome/lg-example-floor-console.yaml` and `esphome compile esphome/lg-example-floor-console.yaml`.
8. Before staging changes, check `git status --short`, `git check-ignore -v esphome/secrets.yaml`, and `git diff`. Check for generated firmware, caches and private logs.
9. Commit locally on the development branch, inspect the staged diff, then push when ready. Remember that **pushing to a public fork publishes that branch**, even if it has not been merged into `main`.

To review/merge into `main`, open a pull request **within your own fork**, selecting base `Adam1986/esphome-lg-controller:main` and compare `feature/lg-controller-v2`. Do not accidentally target the original upstream repository while the changes are still being prepared.

## Notes for public adopters

Use `esphome/components` as the external component directory when installing from the GitHub fork. Test the Git-based `external_components` syntax for your actual ESPHome version before publishing a copy-paste remote install guide. Review both shared YAMLs and select the one relevant to your hardware. Do not apply a hardware example blindly without checking pins, board/chip revision, operating mode and security settings.
