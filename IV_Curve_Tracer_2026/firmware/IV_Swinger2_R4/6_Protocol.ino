/*
 * 6_Protocol.ino
 *
 * Simple serial command reader.
 *
 * This follows the same style as the MPPT firmware:
 *   1. Build a String while characters arrive from Serial.
 *   2. Run the command when a newline is received.
 *   3. Let 7_Protocol_Commands.ino decide what the command means.
 *
 * Command examples:
 *   SWEEP
 *   SWEEP_T 1200 20
 *   RELAY 1 ON
 */

const int COMMAND_MAX_CHARS = 160;
String serialCommand = "";

void protocolBegin() {
  serialCommand.reserve(COMMAND_MAX_CHARS);
  clearCommandArgs();
}

void announceBoot() {
  Serial.println(F("Ready!"));
  Serial.println(F("OK BOOT name=IV_Swinger2_R4 hardware=SSR_MODULE protocol=simple_text baud=115200"));
  Serial.println(F("OK INFO send HELP for student commands, HELP ALL for debug commands"));
}

void protocolPoll() {
  while (Serial.available() > 0) {
    char serialChar = Serial.read();

    if (serialChar == '\n' || serialChar == '\r') {
      if (serialCommand.length() > 0) {
        handleSerialCommand(serialCommand);
        serialCommand = "";
      }
    } else if (serialCommand.length() < COMMAND_MAX_CHARS - 1) {
      serialCommand += serialChar;
    } else {
      Serial.println(F("ERR BUFFER_FULL command_too_long"));
      serialCommand = "";
    }
  }
}

void printHelp(Stream &out, bool includeDebug) {
  out.println(F("OK HELP"));
  out.println(F("NOTE commands_and_ON_OFF_are_case_insensitive"));

  out.println(F("GROUP student"));
  out.print(F("CMD SWEEP  -> auto IV curve, target_points="));
  out.println(MAX_IV_POINTS);
  out.print(F("NOTE SWEEP reserve_points="));
  out.println(SWEEP_OUTPUT_POINT_RESERVE);
  out.print(F("CMD SWEEP_T <points<="));
  out.print(MAX_RAW_POINTS);
  out.println(F("> <sample_delay_us<=1000>  -> manual teaching scan"));
  out.print(F("CMD VOC  -> open-circuit voltage, samples="));
  out.println(VOC_SAMPLE_COUNT);
  out.print(F("CMD ISC  -> short-circuit current, samples="));
  out.println(VOC_SAMPLE_COUNT);
  out.println(F("CMD STATE"));
  out.println(F("CMD IDLE"));
  out.println(F("CMD HELP ALL  -> show teacher/debug commands"));

  if (includeDebug) {
    printDebugHelp(out);
  }

  out.println(F("END_HELP"));
}

void printDebugHelp(Stream &out) {
  out.println(F("GROUP basic_debug"));
  out.println(F("CMD PING"));
  out.println(F("CMD RELAY <1|2|3> <ON|OFF|1|0>"));

  out.println(F("GROUP adc_and_calibration"));
  out.println(F("CMD ADC <0|1> [count]"));
  out.println(F("CMD ADC_BOTH [count]"));
  out.println(F("CMD ADC_SPEED [count]"));
  out.println(F("CMD SPI_HZ <1000000..4000000>"));
  out.println(F("CMD CAL"));
  out.println(F("CMD SET_CAL <name> <value>"));

  out.println(F("GROUP tests"));
  out.println(F("CMD VOC_TEST [count]"));
  out.println(F("CMD ISC_TEST [samples<=1000] [settle_ms<=200]"));

  out.println(F("GROUP sweep_debug"));
  out.print(F("CMD SWEEP_ALL  -> readable auto-sweep summary, max_points="));
  out.println(MAX_IV_POINTS);
  out.println(F("CMD SWEEP_POINTS  -> detailed CSV points from last sweep"));
  out.println(F("CMD STOP"));
}

void printState(Stream &out) {
  out.print(F("STATE spi_hz="));
  out.print(configSpiHz);
  out.print(F(" min_isc_adc="));
  out.print(min_isc_adc);
  out.print(F(" isc_stable_adc="));
  out.print(isc_stable_adc);
  out.print(F(" adc_ref="));
  out.print(adcReferenceVolts, 4);
  out.print(F(" v_scale="));
  out.print(voltageScale, 6);
  out.print(F(" i_scale="));
  out.print(currentScale, 6);
  out.println();
  printRelayState(out);
}
