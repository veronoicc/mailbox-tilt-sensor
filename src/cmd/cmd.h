#pragma once
#include <stddef.h>
#include <stdint.h>
#include "cmd/cmd_parse.h"
// Unified command interpreter shared by the BLE console, the serial CLI and LoRaWAN downlinks.

enum class CmdSource : uint8_t { Ble, Serial, Downlink };
typedef void (*CmdSink)(void* ctx, const char* line);  // receives one reply line (JSON, no trailing newline)
struct CmdOut { CmdSink write; void* ctx; };

// Parse + auth + dispatch; false if rejected/failed. Only CmdSource::Ble requires the PIN.
bool cmdRunLine(const char* line, CmdSource src, const CmdOut& out);
// Opcode lookup + dispatch (data[0] = opcode); false if unknown/empty.
bool cmdRunDownlink(const uint8_t* data, size_t len, const CmdOut& out);
// While lorawanTakeDownlink() yields an FPort 2 frame: cmdRunDownlink (max 4 frames).
void cmdDrainDownlinks(const CmdOut& out);
bool cmdSleepRequested();  // true after the SLEEP command ran
void cmdClearSleepRequest();
