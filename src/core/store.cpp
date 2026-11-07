// Atomic small-file store on InternalFS.
#include "core/store.h"
#include <Arduino.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;
#include <string.h>

bool storeRead(const char* path, void* buf, size_t len) {
  InternalFS.begin();
  File f(path, FILE_O_READ, InternalFS);
  if (!f) return false;
  bool ok = f.size() == len && f.read((uint8_t*)buf, len) == (int)len;
  f.close();
  return ok;
}

// LittleFS rename replaces atomically.
bool storeWrite(const char* path, const void* buf, size_t len) {
  char tmp[40];
  if (strlen(path) + 5 > sizeof(tmp)) return false;
  strcpy(tmp, path);
  strcat(tmp, ".tmp");
  InternalFS.begin();
  InternalFS.remove(tmp);
  File f(tmp, FILE_O_WRITE, InternalFS);
  if (!f) return false;
  bool ok = f.write((const uint8_t*)buf, len) == len;
  f.close();
  return ok && InternalFS.rename(tmp, path);
}
