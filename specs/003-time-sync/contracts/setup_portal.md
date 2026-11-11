# Contract: Phone Setup Portal (S2, FR-004a–c)

Served by the watch only while a setup session is active (≤300 s), on its own access point.

| Item | Value |
| :--- | :--- |
| Access point | SSID `AeroDeck-XXXX` (last 4 hex digits of the MAC), WPA2, one-time 8-char code (alphabet without 0/O/1/l/I) |
| Address | `192.168.4.1`; DNS answers every name with this address (captive-portal behaviour) |
| QR on watch | `WIFI:T:WPA;S:AeroDeck-XXXX;P:<code>;;` |

## HTTP endpoints (port 80)

| Method / path | Response |
| :--- | :--- |
| `GET /` | `text/html` setup page (embedded, ≤6 KB, no external resources). Network list from `/scan.json`, SSID field (editable for hidden networks), password field (`type=password`, maxlength 63), Save button |
| `GET /scan.json` | `application/json` `[{"ssid":"MyNet","rssi":-52,"secure":true}, …]`, strongest first, duplicates merged, ≤20 entries |
| `POST /save` (form: `ssid`, `psk`) | `200 text/html` "Saved, check your watch" on success; `400` with a message if ssid is not 1–32 chars or psk is not 8–63 chars (open networks are not supported) |
| `GET /generate_204`, `/hotspot-detect.html`, `/connecttest.txt`, `/ncsi.txt`, any other path | `302` to `http://192.168.4.1/` |

Rules: the page never contains a saved password. After a successful `/save` the session ends (AP and DNS stop) and the
watch runs the test connection. Requests after the session ends are not answered (AP is off).
