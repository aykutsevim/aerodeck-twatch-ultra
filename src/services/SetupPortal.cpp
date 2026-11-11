#include "services/SetupPortal.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_random.h>
#include <cstring>
#include "core/SettingDefs.h"
#include "services/Log.h"

namespace SetupPortal {

namespace {

// Embedded setup page (contracts/setup_portal.md): no external resources, works on iOS/Android browsers.
const char kPage[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>AeroDeck Wi-Fi</title>
<style>body{font-family:system-ui,sans-serif;background:#0b0f14;color:#e8e8e8;margin:0;padding:16px}
h1{font-size:20px;color:#00ff66}ul{list-style:none;padding:0}li{padding:12px;margin:6px 0;background:#1a222c;
border-radius:8px;display:flex;justify-content:space-between}li.sel{outline:2px solid #00ff66}
input{width:100%;box-sizing:border-box;padding:12px;margin:6px 0 12px;border-radius:8px;border:1px solid #445;
background:#10161d;color:#fff;font-size:16px}button{width:100%;padding:14px;border:0;border-radius:8px;
background:#00ff66;color:#000;font-size:16px;font-weight:600}small{color:#889}#msg{margin-top:12px}</style></head>
<body><h1>AeroDeck &mdash; Wi-Fi setup</h1><small>Pick your network, enter its password, then Save.</small>
<ul id="list"><li>Scanning&hellip;</li></ul>
<form id="f" method="post" action="/save"><label>Network name</label>
<input id="ssid" name="ssid" maxlength="32" required autocomplete="off">
<label>Password</label><input name="psk" type="password" minlength="8" maxlength="63" required>
<button type="submit">Save</button></form><div id="msg"></div>
<script>fetch('/scan.json').then(r=>r.json()).then(n=>{const l=document.getElementById('list');l.innerHTML='';
if(!n.length){l.innerHTML='<li>No networks found &mdash; type the name below</li>';return}
n.forEach(x=>{const li=document.createElement('li');li.textContent=x.ssid;const s=document.createElement('small');
s.textContent=x.rssi+' dBm'+(x.secure?'':' (open)');li.appendChild(s);li.onclick=()=>{
document.querySelectorAll('li').forEach(e=>e.className='');li.className='sel';
document.getElementById('ssid').value=x.ssid};l.appendChild(li)})}).catch(()=>{});</script></body></html>)HTML";

const char kSaved[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Saved</title></head>
<body style="font-family:system-ui,sans-serif;background:#0b0f14;color:#e8e8e8;padding:16px">
<h2 style="color:#00ff66">Saved &mdash; check your watch</h2><p>This hotspot will now close.</p></body></html>)HTML";

constexpr char kCodeAlphabet[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";   // no 0/O/1/I/L
const IPAddress kIp(192, 168, 4, 1);

String s_scanJson;
bool s_saved;
char* s_ssid;
size_t s_ns;
char* s_psk;
size_t s_np;

String jsonEscape(const String& in)
{
    String out;
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (uint8_t(c) < 0x20) {
            out += ' ';
        } else {
            out += c;
        }
    }
    return out;
}

void scanToJson()
{
    int n = WiFi.scanNetworks(false, false);
    s_scanJson = "[";
    int added = 0;
    for (int i = 0; i < n && added < 20; i++) {   // driver returns strongest first
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) {
            continue;   // hidden: typed manually on the page
        }
        bool dup = false;
        for (int j = 0; j < i; j++) {
            if (WiFi.SSID(j) == ssid) {
                dup = true;
                break;
            }
        }
        if (dup) {
            continue;
        }
        if (added++) {
            s_scanJson += ',';
        }
        s_scanJson += "{\"ssid\":\"" + jsonEscape(ssid) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
                      ",\"secure\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
    }
    s_scanJson += "]";
    WiFi.scanDelete();
    LOGI("setup", "scan: %d networks listed", added);
}

void makeCode(char* code, size_t n)
{
    for (size_t i = 0; i + 1 < n; i++) {
        code[i] = kCodeAlphabet[esp_random() % (sizeof kCodeAlphabet - 1)];
    }
    code[n - 1] = '\0';
}

} // namespace

NetResult run(volatile bool& stop, Progress progress, char* ssid, size_t ns, char* psk, size_t np)
{
    s_saved = false;
    s_ssid = ssid;
    s_ns = ns;
    s_psk = psk;
    s_np = np;

    // 1. Scan in STA mode first (the list the phone page shows).
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    scanToJson();

    // 2. Access point with a one-time WPA2 code (shown on the watch only).
    char apSsid[16];
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(apSsid, sizeof apSsid, "AeroDeck-%02X%02X", mac[4], mac[5]);
    char code[9];
    makeCode(code, sizeof code);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(kIp, kIp, IPAddress(255, 255, 255, 0));
    if (!WiFi.softAP(apSsid, code)) {
        LOGE("setup", "softAP failed");
        return NetResult::Cancelled;
    }
    LOGI("setup", "hotspot %s up (code on the watch)", apSsid);   // never log the code

    DNSServer dns;
    dns.start(53, "*", kIp);
    WebServer server(80);
    server.on("/", HTTP_GET, [&server]() { server.send_P(200, "text/html", kPage); });
    server.on("/scan.json", HTTP_GET, [&server]() { server.send(200, "application/json", s_scanJson); });
    server.on("/save", HTTP_POST, [&server]() {
        String s = server.arg("ssid");
        String p = server.arg("psk");
        if (!core::validateString(core::SettingKey::WifiSsid, s.c_str())) {
            server.send(400, "text/plain", "Network name must be 1-32 characters.");
            return;
        }
        if (!core::validateString(core::SettingKey::WifiPsk, p.c_str())) {
            server.send(400, "text/plain", "Password must be 8-63 characters (open networks are not supported).");
            return;
        }
        strncpy(s_ssid, s.c_str(), s_ns - 1);
        s_ssid[s_ns - 1] = '\0';
        strncpy(s_psk, p.c_str(), s_np - 1);
        s_psk[s_np - 1] = '\0';
        server.send_P(200, "text/html", kSaved);
        s_saved = true;
    });
    // Captive-portal probes and anything else → the setup page.
    server.onNotFound([&server]() {
        server.sendHeader("Location", "http://192.168.4.1/", true);
        server.send(302, "text/plain", "");
    });
    server.begin();

    const uint32_t start = millis();
    uint16_t lastReported = 0xFFFF;
    NetResult result = NetResult::Timeout;
    for (;;) {
        dns.processNextRequest();
        server.handleClient();
        const uint32_t elapsed = (millis() - start) / 1000;
        const uint16_t remaining = uint16_t(elapsed >= kSessionS ? 0 : kSessionS - elapsed);
        if (remaining != lastReported) {
            lastReported = remaining;
            progress(apSsid, code, remaining);
        }
        if (s_saved) {
            delay(500);   // let the "Saved" page reach the phone
            server.handleClient();
            result = NetResult::Ok;
            break;
        }
        if (stop) {
            result = NetResult::Cancelled;
            break;
        }
        if (remaining == 0) {
            result = NetResult::Timeout;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    server.stop();
    dns.stop();
    WiFi.softAPdisconnect(true);
    memset(code, 0, sizeof code);
    LOGI("setup", "hotspot closed");
    return result;
}

} // namespace SetupPortal
