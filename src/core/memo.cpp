#include "options.h"
#include "memo.h"
#include <SPIFFS.h>
#include <algorithm>
#include <math.h>
#include <time.h>
#include "config.h"

#define MEMO_FILE "/memo.csv"

Memo memo;

/* UTF-8 -> ASCII (la police de l'ecran ne gere pas tous les accents) */
static String foldAscii(const char* s) {
  String o;
  const uint8_t* p = (const uint8_t*)s;
  while (*p) {
    if (*p < 0x80) { o += (char)*p++; continue; }
    if (*p == 0xC3 && p[1]) {
      const uint8_t c = p[1];
      char r = '?';
      if (c >= 0xA0 && c <= 0xA5) r = 'a'; else if (c == 0xA7) r = 'c';
      else if (c >= 0xA8 && c <= 0xAB) r = 'e'; else if (c >= 0xAC && c <= 0xAF) r = 'i';
      else if (c == 0xB1) r = 'n'; else if (c >= 0xB2 && c <= 0xB6) r = 'o';
      else if (c >= 0xB9 && c <= 0xBC) r = 'u'; else if (c == 0xBD || c == 0xBF) r = 'y';
      else if (c >= 0x80 && c <= 0x85) r = 'A'; else if (c == 0x87) r = 'C';
      else if (c >= 0x88 && c <= 0x8B) r = 'E'; else if (c >= 0x8C && c <= 0x8F) r = 'I';
      else if (c >= 0x92 && c <= 0x96) r = 'O'; else if (c >= 0x99 && c <= 0x9C) r = 'U';
      o += r; p += 2; continue;
    }
    if (*p == 0xC5 && p[1] == 0x93) { o += "oe"; p += 2; continue; }
    if (*p == 0xE2 && p[1] == 0x82 && p[2] == 0xAC) { o += "E"; p += 3; continue; }   // euro
    o += '?';
    p++;
    while ((*p & 0xC0) == 0x80) p++;   // saute les octets de continuation
  }
  return o;
}

static String jsonEsc(const char* s) {
  String o;
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') { o += '\\'; o += *s; }
    else if ((uint8_t)*s < 0x20) o += ' ';
    else o += *s;
  }
  return o;
}

void Memo::_ensure() {
  if (_loaded) return;
  _loaded = true;
  _items.clear();
  if (!SPIFFS.exists(MEMO_FILE)) return;
  File f = SPIFFS.open(MEMO_FILE, "r");
  if (!f) return;
  String csv = f.readString();
  f.close();
  _parse(csv);
}

/* une ligne : AAAA-MM-JJ;HH:MM (ou vide);1 (annuel) ou 0;texte */
bool Memo::_parse(const String& csv) {
  std::vector<MemoItem> v;
  int pos = 0;
  const int len = csv.length();
  while (pos < len && v.size() < MEMO_MAX) {
    int e = csv.indexOf('\n', pos);
    if (e < 0) e = len;
    String line = csv.substring(pos, e);
    pos = e + 1;
    line.trim();
    if (!line.length()) continue;
    const int p1 = line.indexOf(';');
    const int p2 = (p1 < 0) ? -1 : line.indexOf(';', p1 + 1);
    const int p3 = (p2 < 0) ? -1 : line.indexOf(';', p2 + 1);
    if (p3 < 0) continue;
    int y, m, d;
    if (sscanf(line.substring(0, p1).c_str(), "%d-%d-%d", &y, &m, &d) != 3) continue;
    if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) continue;
    MemoItem it;
    memset(&it, 0, sizeof(it));
    it.y = (uint16_t)y; it.m = (uint8_t)m; it.d = (uint8_t)d;
    it.hh = -1; it.mm = -1;
    const String t = line.substring(p1 + 1, p2);
    int hh, mm;
    if (t.length() && sscanf(t.c_str(), "%d:%d", &hh, &mm) == 2 && hh >= 0 && hh < 24 && mm >= 0 && mm < 60) { it.hh = (int8_t)hh; it.mm = (int8_t)mm; }
    it.annual = (line.substring(p2 + 1, p3) == "1");
    String x = line.substring(p3 + 1);
    x.trim();
    x.replace(';', ',');
    if (!x.length()) continue;
    strlcpy(it.txt, x.c_str(), sizeof(it.txt));
    v.push_back(it);
  }
  std::sort(v.begin(), v.end(), [](const MemoItem& a, const MemoItem& b) {
    if (a.y != b.y) return a.y < b.y;
    if (a.m != b.m) return a.m < b.m;
    if (a.d != b.d) return a.d < b.d;
    return (a.hh * 60 + a.mm) < (b.hh * 60 + b.mm);
  });
  _items = v;
  _loaded = true;
  return true;
}

String Memo::toCsv() {
  _ensure();
  String o;
  char buf[24];
  for (const MemoItem& it : _items) {
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u;", (unsigned)it.y, (unsigned)it.m, (unsigned)it.d);
    o += buf;
    if (it.hh >= 0) { snprintf(buf, sizeof(buf), "%02d:%02d", it.hh, it.mm); o += buf; }
    o += ';';
    o += it.annual ? '1' : '0';
    o += ';';
    o += it.txt;
    o += '\n';
  }
  return o;
}

bool Memo::_save() {
  File f = SPIFFS.open(MEMO_FILE, "w");
  if (!f) return false;
  const String csv = toCsv();
  f.print(csv);
  f.close();
  return true;
}

bool Memo::setFromCsv(const String& csv) {
  _parse(csv);
  return _save();
}

/* prochaine occurrence de l'evenement (>= maintenant) ; faux s'il est passe */
static bool nextOccurrence(const MemoItem& it, time_t now, time_t& when, int& dayDiff) {
  struct tm nowTm;
  localtime_r(&now, &nowTm);
  int y = it.annual ? (nowTm.tm_year + 1900) : it.y;
  for (int tries = 0; tries < 2; tries++) {
    struct tm t = {};
    t.tm_year = y - 1900; t.tm_mon = it.m - 1; t.tm_mday = it.d;
    t.tm_hour = (it.hh >= 0) ? it.hh : 0; t.tm_min = (it.hh >= 0) ? it.mm : 0;
    t.tm_isdst = -1;
    when = mktime(&t);
    struct tm e = t;
    e.tm_hour = (it.hh >= 0) ? it.hh : 23; e.tm_min = (it.hh >= 0) ? it.mm : 59; e.tm_sec = 59;
    e.tm_isdst = -1;
    const time_t expire = mktime(&e);   // sans heure : valable toute la journee
    if (expire >= now) {
      struct tm a = nowTm; a.tm_hour = 12; a.tm_min = 0; a.tm_sec = 0; a.tm_isdst = -1;
      struct tm b; localtime_r(&when, &b); b.tm_hour = 12; b.tm_min = 0; b.tm_sec = 0; b.tm_isdst = -1;
      dayDiff = (int)lround(difftime(mktime(&b), mktime(&a)) / 86400.0);
      return true;
    }
    if (!it.annual) return false;
    y++;
  }
  return false;
}

String Memo::_build() {
  _ensure();
  const time_t now = time(NULL);
  struct tm tn;
  localtime_r(&now, &tn);
  if (tn.tm_year + 1900 < 2025) return "";   // heure pas encore synchronisee
  struct Occ { time_t when; int diff; const MemoItem* it; };
  std::vector<Occ> occ;
  for (const MemoItem& it : _items) {
    Occ o;
    o.it = &it;
    if (nextOccurrence(it, now, o.when, o.diff)) occ.push_back(o);
  }
  std::sort(occ.begin(), occ.end(), [](const Occ& a, const Occ& b) { return a.when < b.when; });
  String out;
  char buf[24];
  for (size_t i = 0; i < occ.size() && i < MEMO_SHOWN; i++) {
    const MemoItem& it = *occ[i].it;
    String label;
    if (occ[i].diff <= 0) label = "Auj.";
    else if (occ[i].diff == 1) label = "Demain";
    else {
      struct tm t;
      localtime_r(&occ[i].when, &t);
      snprintf(buf, sizeof(buf), "%02d/%02d", t.tm_mday, t.tm_mon + 1);
      label = buf;
    }
    if (it.hh >= 0) { snprintf(buf, sizeof(buf), " %dh%02d", it.hh, it.mm); label += buf; }
    if (i) out += " ~ ";
    out += label;
    out += ' ';
    out += foldAscii(it.txt);
  }
  return out;
}

void Memo::buildText(char* out, size_t n) {
  _last = _build();
  strlcpy(out, _last.c_str(), n);
}

bool Memo::changed() {
  return _build() != _last;
}

String Memo::toJson() {
  _ensure();
  String o = "{\"show\":";
  o += config.store.showmemo ? "1" : "0";
  o += ",\"max\":" + String(MEMO_MAX) + ",\"textmax\":" + String(MEMO_TEXT_LEN - 1) + ",\"preview\":\"";
  o += jsonEsc(_build().c_str());
  o += "\",\"items\":[";
  char buf[24];
  bool first = true;
  for (const MemoItem& it : _items) {
    if (!first) o += ',';
    first = false;
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u", (unsigned)it.y, (unsigned)it.m, (unsigned)it.d);
    o += "{\"d\":\""; o += buf; o += "\",\"t\":\"";
    if (it.hh >= 0) { snprintf(buf, sizeof(buf), "%02d:%02d", it.hh, it.mm); o += buf; }
    o += "\",\"a\":"; o += it.annual ? "1" : "0";
    o += ",\"x\":\""; o += jsonEsc(it.txt); o += "\"}";
  }
  o += "]}";
  return o;
}
