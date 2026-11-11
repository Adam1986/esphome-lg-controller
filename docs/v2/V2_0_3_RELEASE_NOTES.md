# V2.0.3 Floor Console — candidate Type-D selective read

Experimental change only. Against the V2.0.2 passive-CD experiment, adds `floor_console_selective_cd_probe` (default false). When enabled, after a normal, confirmed-UART-echo 2D discharge write, one candidate `2D.80` frame is transmitted. It reuses the transmitted settings payload to minimise accidental state drift. Requires no active sleep timer and normal 500ms bus-idle scheduling. Wait 10s for two genuine CD reports; full guarded 28 readback remains fallback. The 12-second passive-CD mode is disabled for this trial.

**It is not established that Type-D bit7 is a read flag. This could be a write or change an undocumented setting.** No production deployment, no change to shared standard-unit YAML. See `SELECTIVE_CD_LIVE_PROBE.md` for live test protocol.

Validated by 21 offline C++ regression programmes (including exact captured Bottom frame and checksum), plus Python UI/alias checks. Full ESPHome compile and physical operation remain unverified.
