# Drive logs (SD card, version D)

ESP32-CAM SD recordings from the final track test. Every 0.1 s one row. Analysis: [`docs/REPORT_20261116.en.md`](../REPORT_20261116.en.md) chapter 7 (한국어 [`REPORT_20261116.md`](../REPORT_20261116.md)).
The matching videos (`rec_0011.mjpeg`, `rec_0012.mjpeg`, `rec_0013.mjpeg`, HSV view) are in the release **4wd-car ESP32 Car Control V1.0** (`drive_rec_NNNN.mjpeg`).

| File | Content |
|---|---|
| `log_0011.csv` | 55.6 s : line following, red stop, green go, obstacle stop |
| `log_0012.csv` | 4 min 26 s : red, green, stop signs, obstacles, corners |
| `log_0013.csv` | additional run, 5 min 15 s : yellow (slow) test — no yellow sign was available, so yellow wheels stood in for the yellow signal — plus red, green, stop sign, obstacle |

| Column | Meaning |
|---|---|
| `ms` | ESP32 time since power-on (ms) |
| `frame` | video frame number in `rec_NNNN.mjpeg` (5 per second) |
| `fps` | ESP32 decisions per second |
| `heap` | free internal memory (bytes) |
| `sig_mode` | traffic-light recognition on (1) / off (0) |
| `sig` | decision : 0 none, 1 red, 2 green, 3 yellow, 4 stop sign |
| `red` `green` `yellow` | pixel counts of each colour |
| `fill` `white` | red blob box : % filled with red, % white in its centre (stop sign = fill < 80 and white ≥ 4) |
| `stop_ms` | stop-sign wait left (ms) |
| `bt` | Bluetooth connected to the car |
| `car` | car status line `mode+action,distance,sensors,stage,error,correction` (`PROTOCOL.md`) |
