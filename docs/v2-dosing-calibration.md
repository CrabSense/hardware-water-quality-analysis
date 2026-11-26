# P1.1 — independent dosing pump calibration (bench only)

CH1/2 NO2; CH3/4/5 NO3; CH6/7 NH3/NH4. CH8 unused. Units: integer microlitres (1000 uL = 1 mL) and milliseconds. No actual pump flow, drops or kit dose is assumed. Use clean water for initial bench, then validate compatible actual reagent under real tubing, head, priming and supply conditions before any assay; avoid chemical contamination. Calibrate after tube/pump/reagent changes. Do not use volume values for automated chemistry until verified on hardware. The 5% repeat/verification criteria are provisional engineering acceptance gates, not kit accuracy claims.

## Commissioning
Confirm electrical protection, output polarity, RS485 direction and stable supply first; disconnect unrelated loads. Both boards must run V3. By default outputs and RS485 are disabled; raw calibration pulse additionally needs WQ_ENABLE_CAL_PULSE=1 on both boards, explicitly for supervised bench only. Keep real reagents disconnected during initial test. Use a graduated measuring device or suitable scale with known liquid density, never infer volume from pump timing alone. Prime and remove air first. Choose two measured times within 250..10000 ms, separated by >=500 ms, and a third strictly between. Run each endpoint three separate times, measuring independently. Each pulse must complete without STOP, timeout, reset or lost link; discard incomplete runs. Ensure ARM has time remaining and re-arm only when idle. Calibration itself does not establish physical delivery or elapsed pulse accuracy.

Example syntax only (illustrative numbers, NOT measured CrabSense values):
Master UART: arm; calpulse 1 1000; wait for idle; repeat three times, then 3000 ms three times. For independent check run 2000 ms separately.
Disarm both controllers using master stop and wait for remote state 0. Slave UART:
    cal clear 1
    cal fit 1 1000 990 1000 1010 3000 2980 3000 3020
    cal verify 1 2000 2020
    cal list

Fit invalidates prior active calibration and holds candidate in RAM; verification is required before persistence. Repeat spread (max-min)/mean <=5% for each endpoint; strictly increasing measured means; independent interior measurement error <=5% of model prediction. Valid records stored per channel with schema/channel/CRC and read-back validation. Missing/invalid records rejected on boot. Draft not persisted. Clear failure must be investigated before reboot, since storage may retain previous record. Use verified values only in the original measured interval. No extrapolation, auto splitting or flow feedback.

Master: status shows cal_mask; 0x7f = seven verified channels. After calibration and disarm, disable WQ_ENABLE_CAL_PULSE in normal builds. For a verified CH1, arm and volume 1 2000 (example 2 mL, only if inside actual calibrated range). Slave derives duration from affine interpolation of two measured means; rejects out-of-range or unverified channel and duration beyond ARM lease. ACK means command accepted, not dispensed volume. Raw old dose command removed to avoid ambiguity. Calibration local UART operations allowed only disarmed and camera idle. No recipe execution, calibration expiry enforcement, authorization, automatic chemical decisions, or remote calibration API yet. Revalidate each pump after reboot and repeat physical measurements. Firmware cannot replace overcurrent, flyback, hardware stop and physical safeguards.
