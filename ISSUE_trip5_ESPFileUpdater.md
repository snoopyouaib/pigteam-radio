# Issue pour trip5/ESPFileUpdater (ou trip5/ehRadio)

**Titre :** HTTPS chunked download truncated at ~4 KB with arduino-esp32 core 3.3.x ("Chunked transfer framing invalid or truncated")

**Environment**
- ehRadio 2026.08.19, ESPFileUpdater 1.5.0
- ESP32-S3 N16R8, Arduino core 3.3.7 (PlatformIO, platform espressif32)
- Radio-Browser search (`https://de1.api.radio-browser.info/json/stations/search?...`), any server alias

**Symptom**
```
[ESPFileUpdater: /www/searchresults.json] [Update] Downloading (-1 bytes, length unknown)...
[ESPFileUpdater: /www/searchresults.json] [Error] Chunked transfer framing invalid or truncated.
[ESPFileUpdater: /www/searchresults.json] [Error] Incomplete download (4090 bytes received). Partial file discarded.
```
Always ~4090 (sometimes 3794) bytes, free heap ~110 KB, same on two different networks, SEARCHRESULTS_BUFFER 4 or 32 KB makes no difference. Small downloads (weather, 626 bytes) work.

**Debug output (added prints)**
```
first line=[8000] size=32768
[DBG] readBytes=0 remaining=28678 chunk=32768 written=4090 avail=0 conn=1
```
The first chunk is 32768 bytes, 4090 are read, then `readBytes()` returns 0 with the connection still open.

**Cause**
In core 3.3.x, `NetworkClientSecure::read(uint8_t*, size_t)` returns -1 when `available() <= 0`. `NetworkClient::readBytes()` treats r < 0 as an error and returns immediately (it only waits on r == 0). So on TLS, as soon as the first record has been consumed and the next one has not arrived yet, `readBytes()` returns 0/short. The same applies to the single-byte `stream.read()` used for the CRLF after a chunk (returns -1 when nothing is buffered yet).

**Fix that works for me** (125903 bytes downloaded and verified)
Two small helpers that actually wait for data, replacing `stream.readBytes(...)` (5 places) and `stream.read()` (4 places):
```cpp
static int readByteWait(WiFiClient& s, uint32_t timeoutMs = 5000) {
  uint32_t st = millis();
  while (millis() - st < timeoutMs) {
    int c = s.read();
    if (c >= 0) return c;
    if (!s.connected() && s.available() == 0) return -1;
    delay(1);
  }
  return -1;
}
static size_t readSome(WiFiClient& s, uint8_t* b, size_t len, uint32_t timeoutMs = 15000) {
  uint32_t st = millis();
  while (millis() - st < timeoutMs) {
    int n = s.read(b, len);
    if (n > 0) return (size_t)n;
    if (!s.connected() && s.available() == 0) return 0;
    delay(2);
  }
  return 0;
}
```
In the non-chunked loop, also stop when `contentLength >= 0 && bytesWritten >= contentLength` to avoid waiting for the timeout at the end.

---
## Autres points à signaler (ehRadio)
1. `settings.html` crashes (LoadProhibited) with `DSP_MODEL = DSP_DUMMY`: `vuConf_ptr`, `bufferbarConf_ptr`, `weatherConf_ptr` are NULL in `netserver.cpp` (~lines 537, 539, 554). Fix: add null checks, e.g. `(vuConf_ptr && vuConf_ptr->textsize > 0) || DBGWUI`.
2. `options.h`: `SNTP_1` is `"0.pool.ntp.or"` (missing "g").
3. `SEARCHRESULTS_BUFFER` is 32 KB only if `ARDUINO_ESP32S3_DEV` is defined; ESP32-S3 boards built with another board definition (e.g. esp32-s3-devkitc-1) fall back to 4 KB.
