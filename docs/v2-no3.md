# P1.3 supervised VASTOCEAN NO3

Source: VASTOCEAN manual reproduction, section 4.8, https://manuals.plus/ae/3256812050042028. Not a verified manufacturer-hosted leaflet; confirm kit model, lot, original instructions and water matrix before any reagent use. Described method: rinse vial twice with sample, measure 5 mL, seven drops each liquid 1 and 2 with mixing after each, add one third of supplied powder scoop, shake 10 seconds, seven drops liquid 3 and mix, wait 2 minutes, compare horizontally against white background. No concentration calculated. Do not assume powder is liquid or dispense it through CH5.

## Enable gates
Default WQ_ENABLE_NO3_SUPERVISED=0. Use water and dummy loads first, never unattended. Verify electrical protections, RS485 direction, timing, actual sample volume, exclusive NO3 feed path (including reserve vial isolation), compatible tubing, waste collection, reagent preparations, separate validated lighting and camera/receiver. No selective fill valve, mixer or powder feeder in existing wiring. Verify CH3/4/5 calibration using measured seven-drop equivalents of each actual liquid and measured source/sample/drain times. No assumed microlitres per drop. Source and sample pumps are sequential OUT1/OUT2, no concurrent loads. Operator cleans and rinses vial before start.

Master UART:
    no3 profile SOURCE_MS SAMPLE_MS R1_UL R2_UL R3_UL DRAIN_MS TOTAL_MS
    no3 start
    no3 status

All profile fields are measured integers, RAM only. Exact requested volumes preflight on slave before any fill, no output motion. Confirm isolated vial contains 5 mL with no3 confirm. After CH3 and CH4 dosing, mix manually and confirm each. At powder_add, add measured 1/3 scoop of actual supplied powder manually then no3 confirm to begin timing. Shake continuously by hand for 10 seconds, then no3 confirm. Firmware enforces minimum elapsed interval but cannot detect actual mixing. CH5 then mix and confirm. Reaction 120 s from final confirmation. Each operator wait has 120 s abort limit; this is a software timeout, not analytical tolerance.

At image request, receive JPEG on slave UART with receiver already listening. Validate transport, decode, identity, exposure, timing and image quality; then no3 image ID on master. No automatic proof of saved image. 60 s stage deadline is not a validated colour stability window. USB enumeration may delay exposure; verify against actual leaflet. No automatic OUT8 lighting during camera. Drain OUT4 then OUT7 only after confirmation. Collect waste separately; manually clean vial. STOP aborts with no automatic retry/drain. Inspect before no3 reset, re-enter profile. NO2 and NO3 are mutually exclusive; fault blocks other assay and manual load commands until safe reset. Do not use local slave CLI during assay. V4 unchanged; flash both controllers with this revision. Slave main loop now invokes tickLoads() every iteration, so watchdog and timed pulse receipts are evaluated before subsequent Modbus messages.

Test coverage: ordering, no skipping powder, 10 s minimum, 120 s reaction, timer rollover, profile limits, timeout and interrupted receipt logic. Hardware volume, camera timing, physical interlocks, powder automation, rinsing, actual assay accuracy and simultaneous tests remain unverified.
