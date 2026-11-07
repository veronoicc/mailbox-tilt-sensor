#pragma once
// Persistent mail state (survives power loss), stored in flash file /state.
bool stateMailInside();  // loads lazily, default false
void stateSetMailInside(bool inside);  // writes flash only when the value changes
