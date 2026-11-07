#pragma once
#include <stddef.h>
// Small files in InternalFS, written atomically.
// True only if the file holds exactly len bytes.
bool storeRead(const char* path, void* buf, size_t len);
// Writes path.tmp then renames over path, so a brown-out mid-write leaves the previous file intact.
bool storeWrite(const char* path, const void* buf, size_t len);
