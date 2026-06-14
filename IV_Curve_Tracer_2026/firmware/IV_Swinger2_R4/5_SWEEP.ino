/*
 * 5_SWEEP.ino
 *
 * Public measurement commands and shared sweep state.
 *
 * Normal reading path:
 *   VOC / ISC      -> this file + 5a_Measure.ino
 *   SWEEP          -> this file + 5b_AutoSweep.ino + 5d_Output.ino
 *   SWEEP_T        -> this file + 5c_Sweep_manually.ino + 5d_Output.ino
 *
 * Debugging.ino may inspect these values, but the normal commands above do not
 * depend on any debug-only code.
 */

const int MAX_IV_POINTS = 2500;
const int SWEEP_OUTPUT_POINT_RESERVE = 100;
const int MAX_SWEEP_OUTPUT_POINTS = MAX_IV_POINTS + SWEEP_OUTPUT_POINT_RESERVE;
const int VOC_SAMPLE_COUNT = 500;

// Hard stops for a sweep. The physical sweep stops when the current-channel
// raw ADC reading has been exactly zero for more than 20 consecutive points.
const int MAX_RAW_SWEEP_POINTS_TO_READ = 100000;
const int ZERO_CURRENT_CONFIRM_POINTS = 21;
const uint32_t MAX_SWEEP_ELAPSED_MICROS = 500000UL;
const int SWEEP_TIMEOUT_CHECK_EVERY_POINTS = 16;

// Counts that describe the latest sweep. Raw points are every ADC pair read
// from the MCP3202; output points are the subset saved for printing/plotting.
int sweepOutputPointLimit = MAX_SWEEP_OUTPUT_POINTS;
int sweepRawPointCount = 0;
int sweepOutputPointCount = 0;

// Endpoint measurements in raw ADC counts.
int sweepVocAdcCount = 0;
int sweepIscAdcCount = 0;

int zeroCurrentConfirmCount = 0;

bool sweepReachedEnd = false;
bool sweepCurrentReachedZero = false;
bool sweepIscReady = false;
bool sweepSaveAllRawPoints = false;

// Timing and save strategy for the latest sweep.
uint32_t sweepElapsedMicros = 0;
uint32_t sweepSaveIntervalMicros = 1;
int sweepManualDelayMicros = -1;  // -1 means normal SWEEP, 0..1000 means SWEEP_T.

RawPoint latestSweepPoint;

void handleVoc() {
  measureVocForSweep();
  Serial.println(smartAdcToVolts(lastVocAdc), 4);
}

void handleIsc() {
  measureVocForSweep();
  if (!measureStableIscForCommand()) {
    Serial.println(F("ERR ISC not_stable"));
    return;
  }

  Serial.println(smartAdcToAmps(lastIscAdc), 4);
}

void handleSweep() {
  runAutomaticSweep(Serial, MAX_IV_POINTS, false);
}

void handleSweepT() {
  int points = MAX_IV_POINTS;
  if (!parseCountOrReport(Serial, nextArg(), MAX_IV_POINTS, 1,
                          MAX_RAW_POINTS, &points, F("points"))) {
    return;
  }

  int sampleDelayMicros = 0;
  if (!parseCountOrReport(Serial, nextArg(), 0, 0, 1000, &sampleDelayMicros,
                          F("sample_delay_us"))) {
    return;
  }

  runTeachingSweep(Serial, points, sampleDelayMicros);
}
