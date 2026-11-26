# P1.5 supervised batch coordinator
Manual bitmask: 1 NO2, 2 NO3, 4 NH3; 3/5/6 combinations; 7 all. Scheduled request requires all 3; mode tag only, Kiosk will initiate schedules. Default WQ_ENABLE_BATCH_SUPERVISED=0. Enable selected individual flags and outputs/RS485 only after safe isolated dummy-load commissioning. Original assay documentation and leaflet/calibration requirements apply. Before start stage selected profiles with existing no2/no3/nh3 profile commands, all selected total drain times equal. Exact volumes checked before any movement. Physically isolate feed to each selected vial; no automatic selector, no filling reserve vial 4; independently stable lighting; collect waste safely. Do not operate unattended.
Commands:
- batch start manual MASK
- batch start scheduled
- batch status
- batch route ID (1 NO2, 2 NO3, 3 NH3), only after route prompt and verified isolated feed.
- batch confirm ID at confirm_sample (5 mL), mix1/2/3 or powder_add/powder_mix (NO3); shake powder at least 10 seconds.
- batch optical ID at optical prompt, after correct vial positioning and stable light.
- batch image ID CAPTURE_ID after CRC/decode, correct vial, exposure/timing and image verification; slave UART receiver must already listen.
- stop or batch stop; batch reset after safe recovery/cleaning with fresh idle link.
NH3 then NO2 then NO3 preparation, single actuator owner. Reaction timers overlap independently. Prep budget 90 s, route prompt 120 s, pending image deadline guard 100 s. Final confirmation faults on deadlines less than 70 s apart. Capture dispatch within 10 s of due time, 60 s image stage. These are provisional software timeouts, NOT validated chemistry tolerances. USB enumeration may delay exposure. No auto retry or extending timer. All selected images verified before drains OUT3/4/5 serially, shared OUT7 once. No OUT6 or CH8. No automatic rinse yet.
Batch and session IDs are boot-local random identifiers; BATCH IMAGE JSON links capture request, not actual exposure. NH3 existing SAMPLE/IMAGE_LINK retained. Gateway must correlate board boot/UTC/image. Fault aborts whole batch and disarms best effort; no automatic drain/retry. Evidence retained until explicit reset; profiles cleared. Single-assay controls blocked while batch held. No real hardware validation or automatic powder/stirring/route/lighting; do not use slave CLI during batch. V4 unchanged. Tests exercise selection, overlapping reactions, deadline collision, reset, drain and rollover; actual integration timing pending.
