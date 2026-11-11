# Contract: Weather Provider Exchange (S3)

**Endpoint**: `https://aviationweather.gov/api/data/metar?ids=<ICAO>&format=raw` (ADR-007 default provider, no key).

## Request (written by WeatherService over `WiFiClientSecure`)
```text
GET /api/data/metar?ids=LTAC&format=raw HTTP/1.1\r\n
Host: aviationweather.gov\r\n
User-Agent: AeroDeck/<fw version>\r\n
Accept: text/plain\r\n
Connection: close\r\n
\r\n
```

## TLS
- Server certificate verified against the built-in roots (`src/services/WxCerts.h`): DigiCert Global Root G2 and
  DigiCert TLS RSA4096 Root G5 (fallback).
- SNI `aviationweather.gov`. Hostname verification is on (WiFiClientSecure default with a CA set).
- A failed handshake or verification → `NetFail` (log `tls err=<code>`).

## Responses handled

| Response | Meaning | Result |
|---|---|---|
| `200`, `Content-Length` ≤ 2048, `text/plain`, ≥1 line `[METAR\|SPECI ]ICAO DDHHMMZ …` | Report(s), newest first | Ok (first line whose station matches) |
| `200` with an empty body, or only `NIL` | No report | NoReport |
| `204 No Content` | Unknown station or no recent report (observed for `ZZZZ`) | NoReport |
| `200` with `Transfer-Encoding: chunked`, or no `Content-Length` and the body > 2048 | Unexpected | ServerError |
| `3xx/4xx/5xx` | Error | ServerError (`http=<code>`) |
| Body line station ≠ requested, or no `DDHHMMZ` | Unexpected | ServerError |
| No complete response before the deadline | Timeout | NetFail |

Observed sample (2026-11-11):
`HTTP/1.1 200 OK`, `Content-Type: text/plain; charset=utf-8`, `Content-Length: 63`, and the body
`METAR LTAC 111250Z 27007KT 9999 FEW016 BKN030 10/04 Q1020 NOSIG`.
