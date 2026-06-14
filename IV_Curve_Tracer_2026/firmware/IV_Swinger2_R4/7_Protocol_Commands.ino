/*
 * 7_Protocol_Commands.ino
 *
 * Fixed command list for the R4 firmware.
 *
 * Commands are simple text:
 *   WORD
 *   WORD number
 *   WORD number number
 *
 * The command table below keeps the serial protocol easy to scan. Adding a
 * command should usually mean adding one row and one handler.
 */

String commandName = "";
String commandArgs = "";
const int MAX_COMMAND_ARGS_CHARS = 120;
char commandArgsBuffer[MAX_COMMAND_ARGS_CHARS];
char *nextArgContext = nullptr;
bool nextArgStarted = false;

typedef void (*CommandHandler)();

struct SerialCommand {
  const char *name;
  CommandHandler handler;
};

const SerialCommand COMMANDS[] = {
    {"HELP", handleHelp},
    {"PING", handlePing},
    {"STATE", handleState},
    {"IDLE", handleIdle},
    {"RELAY", handleRelay},
    {"ADC", handleAdc},
    {"ADC_BOTH", handleAdcBoth},
    {"ADC_SPEED", handleAdcSpeed},
    {"SPI_HZ", handleSpiHz},
    {"CAL", handleCal},
    {"SET_CAL", handleSetCal},
    {"VOC", handleVoc},
    {"ISC", handleIsc},
    {"VOC_TEST", handleVocTest},
    {"ISC_TEST", handleIscTest},
    {"SWEEP", handleSweep},
    {"SWEEP_T", handleSweepT},
    {"SWEEP_ALL", handleSweepAll},
    {"SWEEP_POINTS", handleSweepPoints},
    {"STOP", handleStop},
};

void handleSerialCommand(String commandLine) {
  commandLine.trim();
  commandLine.toUpperCase();
  if (commandLine.length() == 0) {
    return;
  }

  splitCommandLine(commandLine);

  if (!dispatchCommand()) {
    Serial.print(F("ERR UNKNOWN_COMMAND cmd="));
    Serial.println(commandName);
  }

  clearCommandArgs();
}

bool dispatchCommand() {
  for (const SerialCommand &cmd : COMMANDS) {
    if (commandName == cmd.name) {
      cmd.handler();
      return true;
    }
  }

  return false;
}

void splitCommandLine(String commandLine) {
  int spaceIndex = commandLine.indexOf(' ');

  if (spaceIndex < 0) {
    commandName = commandLine;
    commandArgs = "";
    return;
  }

  commandName = commandLine.substring(0, spaceIndex);
  commandArgs = commandLine.substring(spaceIndex + 1);
  commandArgs.trim();
}

void clearCommandArgs() {
  commandName = "";
  commandArgs = "";
  resetNextArg();
}

void resetNextArg() {
  commandArgsBuffer[0] = '\0';
  nextArgContext = nullptr;
  nextArgStarted = false;
}

char *nextArg() {
  if (!nextArgStarted) {
    commandArgs.toCharArray(commandArgsBuffer, sizeof(commandArgsBuffer));
    nextArgContext = nullptr;
    nextArgStarted = true;
    return strtok_r(commandArgsBuffer, " ", &nextArgContext);
  }

  return strtok_r(nullptr, " ", &nextArgContext);
}

bool equalsIgnoreCase(const char *a, const char *b) {
  return strcasecmp(a, b) == 0;
}

bool parseOnOff(const char *text, bool *value) {
  if (!text) {
    return false;
  }
  if (equalsIgnoreCase(text, "ON") || strcmp(text, "1") == 0) {
    *value = true;
    return true;
  }
  if (equalsIgnoreCase(text, "OFF") || strcmp(text, "0") == 0) {
    *value = false;
    return true;
  }
  return false;
}

bool parseChannel(const char *text, int *channel) {
  return parseIntInRange(text, ADC_VOLTAGE_CH, ADC_CURRENT_CH, channel);
}

bool parseIntInRange(const char *text, int minValue, int maxValue, int *value) {
  if (!text || !*text) {
    return false;
  }

  char *end = nullptr;
  const long parsed = strtol(text, &end, 10);
  if (*end != '\0' || parsed < minValue || parsed > maxValue) {
    return false;
  }

  *value = static_cast<int>(parsed);
  return true;
}

bool parseUint32InRange(const char *text, uint32_t minValue,
                        uint32_t maxValue, uint32_t *value) {
  if (!text || !*text || text[0] == '-') {
    return false;
  }

  char *end = nullptr;
  const unsigned long parsed = strtoul(text, &end, 10);
  if (*end != '\0' || parsed < minValue || parsed > maxValue) {
    return false;
  }

  *value = static_cast<uint32_t>(parsed);
  return true;
}

bool parseFloatValue(const char *text, float *value) {
  if (!text || !*text) {
    return false;
  }

  char *end = nullptr;
  const float parsed = strtod(text, &end);
  if (*end != '\0') {
    return false;
  }

  *value = parsed;
  return true;
}

bool parseCountOrReport(Stream &out, const char *text, int fallback,
                        int minValue, int maxValue, int *value,
                        const __FlashStringHelper *name) {
  if (!text) {
    *value = fallback;
    return true;
  }

  if (parseIntInRange(text, minValue, maxValue, value)) {
    return true;
  }

  out.print(F("ERR BAD_ARG "));
  out.print(name);
  out.print(F("_range_"));
  out.print(minValue);
  out.print(F("_"));
  out.println(maxValue);
  return false;
}

// ---------------------------------------------------------------------------
// Student/basic commands: start here for daily use.
// ---------------------------------------------------------------------------

void handleHelp() {
  char *mode = nextArg();
  printHelp(Serial, mode && equalsIgnoreCase(mode, "ALL"));
}

void handleState() {
  printState(Serial);
}

void handlePing() {
  Serial.println(F("OK PONG"));
}

void handleIdle() {
  setIdleState();
  Serial.println(F("OK IDLE"));
  printState(Serial);
}

void handleStop() {
  setIdleState();
  Serial.println(F("OK STOP"));
  printState(Serial);
}
