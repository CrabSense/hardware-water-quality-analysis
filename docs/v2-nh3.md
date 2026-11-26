# P1.4 supervised NH3/NH4 Thuy Phuoc

Candidate method from retailer listing https://tecki.vn/p1.188708905.6944229731-dung-dich-kiem-tra-nong-do-amoniac-test-nh3-nh4-cho-ho-ca-canh-ho-thuy-sinh (product attributed to Thuy Phuoc, instructions at lines 53-59), and product identity/two bottles corroborated at https://cacung.vn/dung-dich-do-kiem-tra-nh3-nh4-ho-ca-canh-thuy-phuoc. These are secondary sources, not manufacturer-controlled instructions. Verify actual kit revision, lot, colour scale, water matrix/salinity compatibility and SDS before reagent use. Candidate method: rinse with sample, 5 mL sample, five drops I and mix, five drops II and mix, compare colour horizontally after approximately 10 min. This firmware uses 600 s from final mixing confirmation as commissioning target, not validated analytical timing. No automatic inference of TAN, NH3, NH4, nitrogen mass units, salinity or toxicity from images.

## Preconditions

WQ_ENABLE_NH3_SUPERVISED defaults to 0. Use dummy loads and clean water first. Verify electrical protections, STOP, RS485 direction, physical OFF times, hydraulic isolation of NH3 vial and reserve vial, chemical-compatible tubing and waste collection, measured sample volume, independent stable lighting and camera receiver. Shared feed may make selective sampling impossible; do not enable without proven isolation. CH6/7 require verified calibrations in range for measured five-drop equivalents of actual bottles I/II. No assumed uL/drop. All times for OUT1/OUT2/OUT5/OUT7 measured; profile RAM only. Operator cleans and rinses vial before start. Do not operate unattended or reuse samples/waste in farm water.

Master UART:
    nh3 profile SOURCE_MS SAMPLE_MS R1_UL R2_UL DRAIN_MS TOTAL_MS
    nh3 start
    nh3 status

Preflight checks both exact volumes on slave without moving outputs. OUT1 then OUT2; operator verifies 5 mL in isolated vial, then nh3 confirm. Dose CH6, mix manually, nh3 confirm; dose CH7, mix manually, nh3 confirm. Firmware starts 600-second nonblocking reaction timer. Operator confirmations expire at 120 s (software safety bound, not assay tolerance). Dose completion receipt is not physical measured volume.

At image request, receiver must already listen to slave UART; verify JPEG identity, CRC, decode, time, exposure, white background and viewing geometry against kit instructions. Enter nh3 image ID only after successful validation. Image stage expires after 60 s; actual exposure can be delayed by USB initialization. No automatic proof of saved image or validated colour window, no OUT8 lighting during USB session. OUT5 then OUT7 only after image confirmation. Manual post-test cleaning, capture waste safely per SDS.

STOP, link loss during actuation, invalid receipt, unexpected remote state or timeout => fault, best-effort stop, no automatic retry or drain. Inspect physically, establish link and idle outputs, clean and recover, then nh3 reset. All three assays keep exclusive ownership from start through complete/fault until explicit reset. Other assays and manual controls blocked; use owning assay status/reset and STOP. Do not issue separate slave CLI commands during a run. No simultaneous assay scheduling, automated mixing/lighting, salinity correction, AI result or real hardware validation. Modbus V4 unchanged. Host tests cover mapping, shared ownership, timer rollover, incomplete confirmations, bounds, 600 s and preserved NO2 300 s timing.
