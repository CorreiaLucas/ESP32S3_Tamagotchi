// ==========================================================================
//  ESP32-S3 Digimon Pet  --  sketch entry point.
//
//  All game logic lives in the table-driven state machine (GameStateMachine.*).
//  This file is intentionally tiny: it only wires the Arduino setup()/loop()
//  lifecycle to the state machine's entry points. See GameStateMachine.h for a
//  description of how states, entry hooks, and transitions work.
// ==========================================================================
#include "GameStateMachine.h"

void setup() {
  gsmSetup();
}

void loop() {
  gsmLoop();
}
