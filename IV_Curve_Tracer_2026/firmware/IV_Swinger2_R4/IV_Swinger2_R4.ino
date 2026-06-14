/*
 * IV_Swinger2_R4.ino
 *
 * Main entry point. This file only contains setup() and loop().
 *
 * Code organization:
 *   Each .ino file is one Arduino IDE tab/section.
 *   Read in roughly this order: hardware -> measurement -> sweep -> output -> serial commands -> debug tools.
 *
 * Suggested reading order:
 *   2_Config.ino            Pin map, defaults, calibration, shared state
 *   3_ADC.ino               MCP3202 ADC reads
 *   4_Relay.ino             SSR relay control
 *   5_SWEEP.ino             Public VOC/ISC/SWEEP/SWEEP_T commands
 *   5a_Measure.ino          Shared VOC / ISC measurement helpers
 *   5b_AutoSweep.ino        Automatic SWEEP flow
 *   5c_Sweep_manually.ino   Manual SWEEP_T flow
 *   5d_Output.ino           IV output and ADC conversion
 *   6_Protocol.ino          Simple text serial input, HELP/STATE
 *   7_Protocol_Commands.ino Daily-use command handlers and argument parsing
 *   Debugging.ino           Development-only debug/test tools
 */

#include <SPI.h>

#define SERIAL_BAUD 115200

void setup() {
  // USB serial is the only current communication link between the R4 and the PC.
  Serial.begin(SERIAL_BAUD);
  while (!Serial) {
    ; // Wait for Serial Monitor so the boot message is visible.
  }

  adcBegin();
  relaysBegin();
  protocolBegin();
  announceBoot();
}

void loop() {
  protocolPoll();
}
