#include "cmd/cmd_parse.h"
#include <string.h>

static bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static char upper(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c; }

// Case-insensitive match of the first n chars of s against the upper-case literal tag.
static bool hasTag(const char* s, size_t len, const char* tag) {
  size_t n = strlen(tag);
  if (len < n) return false;
  for (size_t i = 0; i < n; i++) {
    if (upper(s[i]) != tag[i]) return false;
  }
  return true;
}

// Copies len chars plus NUL; false if it does not fit.
static bool copyField(char* dst, size_t cap, const char* s, size_t len, bool up) {
  if (len >= cap) return false;
  for (size_t i = 0; i < len; i++) dst[i] = up ? upper(s[i]) : s[i];
  dst[len] = 0;
  return true;
}

bool cmdParseLine(const char* line, ParsedLine& out) {
  memset(&out, 0, sizeof out);
  if (!line) return false;

  size_t total = 0;
  while (line[total]) {
    if (++total > CMD_LINE_MAX) return false;
  }

  const char* p = line;
  for (;;) {
    while (isSpace(*p)) p++;
    if (!*p) break;
    const char* s = p;
    while (*p && !isSpace(*p)) p++;
    size_t len = (size_t)(p - s);

    bool ok;
    if (hasTag(s, len, "PIN:")) {
      ok = !out.hasPin && copyField(out.pin, sizeof out.pin, s + 4, len - 4, false);
      out.hasPin = true;
    } else if (hasTag(s, len, "CMD:")) {
      ok = !out.name[0] && len > 4 && copyField(out.name, sizeof out.name, s + 4, len - 4, true);
    } else if (!out.name[0] && !out.hasPin) {
      ok = copyField(out.name, sizeof out.name, s, len, true);  // CLI form: first bare token is the command
    } else {
      ok = out.argc < 4 && copyField(out.args[out.argc], sizeof out.args[0], s, len, false);
      if (ok) out.argc++;
    }
    if (!ok) {
      memset(&out, 0, sizeof out);
      return false;
    }
  }

  if (!out.name[0]) {
    memset(&out, 0, sizeof out);
    return false;
  }
  return true;
}
