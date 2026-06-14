/*
 * Debugging.ino
 *
 * Debug-only command handlers:
 *   RELAY           Manually switch SSR outputs
 *   ADC / ADC_BOTH  Inspect raw ADC readings
 *   ADC_SPEED       Measure ADC pair-read speed
 *   SPI_HZ          Change MCP3202 SPI clock
 *   CAL / SET_CAL   Inspect and tune conversion constants
 *   VOC_TEST        Measure Voc and current-channel noise
 *   ISC_TEST        Short-circuit current sampling test
 *   SWEEP_ALL       Auto sweep with readable debug summary
 *   SWEEP_POINTS    Detailed CSV from the last formal sweep
 */

// ---------------------------------------------------------------------------
// Manual relay path commands
// ---------------------------------------------------------------------------

void handleRelay() {
  char *relayArg = nextArg();
  char *stateArg = nextArg();
  if (!relayArg || !stateArg) {
    Serial.println(F("ERR BAD_ARG usage=\"RELAY <1|2|3> <ON|OFF|1|0>\""));
    return;
  }

  int relayNumber = 0;
  if (!parseIntInRange(relayArg, 1, 3, &relayNumber)) {
    Serial.println(F("ERR BAD_ARG relay_must_be_1_2_3"));
    return;
  }

  bool active = false;
  if (!parseOnOff(stateArg, &active)) {
    Serial.println(F("ERR BAD_ARG relay_state_must_be_ON_OFF_1_0"));
    return;
  }

  if (relayNumber == 1) {
    setSsr1(active);
  } else if (relayNumber == 2) {
    setSsr2(active);
  } else {
    setSsr3(active);
  }

  Serial.print(F("OK RELAY id="));
  Serial.print(relayNumber);
  Serial.print(F(" state="));
  Serial.println(active ? F("ON") : F("OFF"));
  printRelayState(Serial);
}

// ---------------------------------------------------------------------------
// ADC, SPI, and calibration commands
// ---------------------------------------------------------------------------

void handleAdc() {
  char *channelArg = nextArg();
  char *countArg = nextArg();

  int channel = 0;
  if (!parseChannel(channelArg, &channel)) {
    Serial.println(F("ERR BAD_ARG channel_must_be_0_or_1"));
    return;
  }

  int count = 1;
  if (!parseCountOrReport(Serial, countArg, 1, 1, 5000, &count, F("count"))) {
    return;
  }

  uint32_t sum = 0;
  int minValue = ADC_SAT;
  int maxValue = 0;
  int satCount = 0;
  for (int sample = 0; sample < count; ++sample) {
    int value = readAdc(channel);
    sum += value;
    minValue = min(minValue, value);
    maxValue = max(maxValue, value);
    if (value >= ADC_SAT) {
      satCount++;
    }
  }

  Serial.print(F("ADC ch="));
  Serial.print(channel);
  Serial.print(F(" n="));
  Serial.print(count);
  Serial.print(F(" min="));
  Serial.print(minValue);
  Serial.print(F(" avg="));
  Serial.print(static_cast<float>(sum) / count, 2);
  Serial.print(F(" max="));
  Serial.print(maxValue);
  Serial.print(F(" sat="));
  Serial.println(satCount);
}

void handleAdcBoth() {
  int count = 1;
  if (!parseCountOrReport(Serial, nextArg(), 1, 1, 5000, &count, F("count"))) {
    return;
  }

  uint32_t vSum = 0;
  uint32_t iSum = 0;
  int vMin = ADC_SAT;
  int iMin = ADC_SAT;
  int vMax = 0;
  int iMax = 0;
  int vSat = 0;
  int iSat = 0;

  for (int sample = 0; sample < count; ++sample) {
    int v = readAdc(ADC_VOLTAGE_CH);
    int i = readAdc(ADC_CURRENT_CH);
    vSum += v;
    iSum += i;
    vMin = min(vMin, v);
    iMin = min(iMin, i);
    vMax = max(vMax, v);
    iMax = max(iMax, i);
    if (v >= ADC_SAT) {
      vSat++;
    }
    if (i >= ADC_SAT) {
      iSat++;
    }
  }

  Serial.print(F("ADC_BOTH n="));
  Serial.print(count);
  Serial.print(F(" ch0_min="));
  Serial.print(vMin);
  Serial.print(F(" ch0_avg="));
  Serial.print(static_cast<float>(vSum) / count, 2);
  Serial.print(F(" ch0_max="));
  Serial.print(vMax);
  Serial.print(F(" ch0_sat="));
  Serial.print(vSat);
  Serial.print(F(" ch1_min="));
  Serial.print(iMin);
  Serial.print(F(" ch1_avg="));
  Serial.print(static_cast<float>(iSum) / count, 2);
  Serial.print(F(" ch1_max="));
  Serial.print(iMax);
  Serial.print(F(" ch1_sat="));
  Serial.println(iSat);
}

void handleAdcSpeed() {
  int count = 1000;
  if (!parseCountOrReport(Serial, nextArg(), 1000, 1, 50000, &count,
                          F("count"))) {
    return;
  }

  int lastVoltage = 0;
  int lastCurrent = 0;
  const uint32_t startUsecs = micros();
  for (int sample = 0; sample < count; ++sample) {
    lastCurrent = readAdc(ADC_CURRENT_CH);
    lastVoltage = readAdc(ADC_VOLTAGE_CH);
  }
  const uint32_t elapsedUsecs = micros() - startUsecs;
  const float usecsPerPair = static_cast<float>(elapsedUsecs) / count;
  const float pairsPerSecond = elapsedUsecs
                                   ? (1000000.0f * count) / elapsedUsecs
                                   : 0.0f;

  Serial.print(F("ADC_SPEED n="));
  Serial.print(count);
  Serial.print(F(" elapsed_us="));
  Serial.print(elapsedUsecs);
  Serial.print(F(" us_per_pair="));
  Serial.print(usecsPerPair, 2);
  Serial.print(F(" pairs_per_sec="));
  Serial.print(pairsPerSecond, 1);
  Serial.print(F(" channel_reads_per_sec="));
  Serial.print(pairsPerSecond * 2.0f, 1);
  Serial.print(F(" last_ch0="));
  Serial.print(lastVoltage);
  Serial.print(F(" last_ch1="));
  Serial.println(lastCurrent);
}

void handleSpiHz() {
  char *hzArg = nextArg();
  if (!hzArg) {
    Serial.println(F("ERR BAD_ARG usage=\"SPI_HZ <1000000..4000000>\""));
    return;
  }

  uint32_t hz = 0;
  if (!parseUint32InRange(hzArg, 1000000UL, 4000000UL, &hz)) {
    Serial.println(F("ERR BAD_ARG spi_hz_range_1000000_4000000"));
    return;
  }

  setAdcSpiHz(hz);
  Serial.print(F("OK SPI_HZ hz="));
  Serial.println(hz);
}

void handleCal() {
  printCalibration(Serial);
}

void handleSetCal() {
  char *name = nextArg();
  char *valueText = nextArg();

  float value = 0.0f;
  if (!name || !parseFloatValue(valueText, &value)) {
    Serial.println(F("ERR BAD_ARG usage=\"SET_CAL <name> <value>\""));
    printCalibrationNames(Serial);
    return;
  }

  if (!setCalibrationValue(name, value)) {
    Serial.print(F("ERR BAD_ARG unknown_cal_name="));
    Serial.println(name);
    printCalibrationNames(Serial);
    return;
  }

  Serial.print(F("OK SET_CAL name="));
  Serial.print(name);
  Serial.print(F(" value="));
  Serial.println(value, 6);
  printCalibration(Serial);
}

// ---------------------------------------------------------------------------
// VOC / ISC test command handlers
// ---------------------------------------------------------------------------

void handleVocTest() {
  int loops = 400;
  if (!parseCountOrReport(Serial, nextArg(), 400, 1, 2000, &loops,
                          F("count"))) {
    return;
  }
  Serial.println(F("OK VOC_TEST begin"));
  measureVocAndNoiseFloor(Serial, loops);
  Serial.println(F("OK VOC_TEST end"));
}

void handleIscTest() {
  int samples = 400;
  int settleMs = 80;
  if (!parseCountOrReport(Serial, nextArg(), 400, 1, 1000, &samples,
                          F("samples"))) {
    return;
  }
  if (!parseCountOrReport(Serial, nextArg(), 80, 0, 200, &settleMs,
                          F("settle_ms"))) {
    return;
  }
  Serial.println(F("OK ISC_TEST begin"));
  shortIscTest(Serial, samples, settleMs);
  Serial.println(F("OK ISC_TEST end"));
}

// ---------------------------------------------------------------------------
// Formal sweep debug commands
// ---------------------------------------------------------------------------

void handleSweepAll() {
  runAutomaticSweep(Serial, MAX_IV_POINTS, true);
}

void handleSweepPoints() {
  printDetailedSweepData(Serial);
}

void printSweepAllSummary(Stream &out) {
  out.println(F("SAVE_MODE"));
  out.print(F("  mode="));
  out.println(sweepSaveAllRawPoints ? F("all_raw") : F("time_interval"));
  out.print(F(" save_interval_us="));
  out.println(sweepSaveIntervalMicros);

  out.println(F("FORMAL"));
  printSweepResultSummary(out, "  ");
}

void printSweepPassSummary(Stream &out, const char *prefix) {
  out.print(prefix);
  out.print(F("complete="));
  out.print(sweepReachedEnd ? 1 : 0);
  out.print(F(" current_tail="));
  out.print(sweepCurrentReachedTail ? 1 : 0);
  out.print(F(" isc_stable="));
  out.println(sweepIscReady ? 1 : 0);

  out.print(prefix);
  out.print(F("measured="));
  out.print(sweepRawPointCount);
  out.print(F(" elapsed_us="));
  out.print(sweepElapsedMicros);
  out.print(F(" us_per_raw="));
  out.println(rawUsecsPerPoint(), 2);

  out.print(prefix);
  out.print(F("voc_adc="));
  out.print(sweepVocAdcCount);
  out.print(F(" isc_adc="));
  out.print(sweepIscAdcCount);
  out.print(F(" done_i_adc="));
  out.print(sweepEndCurrentAdcThreshold);
  out.print(F(" done_delta_adc="));
  out.println(SWEEP_DONE_CURRENT_DELTA_ADC);
}

void printSweepResultSummary(Stream &out, const char *prefix) {
  out.print(prefix);
  out.print(F("complete="));
  out.print(sweepReachedEnd ? 1 : 0);
  out.print(F(" current_tail="));
  out.print(sweepCurrentReachedTail ? 1 : 0);
  out.print(F(" saved="));
  out.print(sweepOutputPointCount);
  out.print(F(" target="));
  out.print(sweepOutputPointLimit);
  out.print(F(" measured="));
  out.println(sweepRawPointCount);

  out.print(prefix);
  out.print(F("save_all_raw="));
  out.print(sweepSaveAllRawPoints ? 1 : 0);
  out.print(F(" timeout_check_every="));
  out.println(SWEEP_TIMEOUT_CHECK_EVERY_POINTS);

  out.print(prefix);
  out.print(F("elapsed_us="));
  out.print(sweepElapsedMicros);
  out.print(F(" us_per_raw="));
  out.print(rawUsecsPerPoint(), 2);
  out.print(F(" raw_per_sec="));
  out.println(rawPointsPerSecond(), 1);

  out.print(prefix);
  out.print(F("complete="));
  out.println(sweepReachedEnd ? 1 : 0);
}

float rawUsecsPerPoint() {
  if (sweepRawPointCount <= 0) {
    return 0.0f;
  }
  return (float)sweepElapsedMicros / sweepRawPointCount;
}

float rawPointsPerSecond() {
  if (sweepElapsedMicros <= 0) {
    return 0.0f;
  }
  return (1000000.0f * sweepRawPointCount) / sweepElapsedMicros;
}

void printDetailedSweepData(Stream &out) {
  if (sweepOutputPointCount <= 0) {
    out.println(F("ERR SWEEP_POINTS no_sweep_data"));
    return;
  }

  out.println(F("BEGIN_SWEEP_POINTS format=\"index,volts,amps,adc_v,adc_i_raw,adc_i_corr\""));
  for (int index = 0; index < sweepOutputPointCount; ++index) {
    printDetailedSweepPoint(out, index);
  }
  out.println(F("END_SWEEP_POINTS"));
}

void printDetailedSweepPoint(Stream &out, int index) {
  const RawPoint point = scratch.rawPoints[index];
  const int correctedCurrentAdc = correctedSweepCurrentAdc(index);
  const float amps = smartAdcToAmps(correctedCurrentAdc);
  const float volts = smartAdcToVolts(point.v);

  out.print(index);
  out.print(F(","));
  out.print(volts, 4);
  out.print(F(","));
  out.print(amps, 4);
  out.print(F(","));
  out.print(point.v);
  out.print(F(","));
  out.print(point.i);
  out.print(F(","));
  out.println(correctedCurrentAdc);
}

// ---------------------------------------------------------------------------
// VOC / ISC test 
// ---------------------------------------------------------------------------

void measureVocAndNoiseFloor(Stream &out, int loops) {
  setIdleState();
  delay(20);
  measureVocAverage(loops);
  printVocNoise(out);
}

void shortIscTest(Stream &out, int requestedSamples, int settleMillis) {
  const int sampleCount = constrain(requestedSamples, 1, 1000);
  const int settleMs = constrain(settleMillis, 0, 200);
  setIdleState();
  delay(10);

  uint32_t vSum = 0;
  uint32_t iSum = 0;
  int vMin = ADC_SAT;
  int iMin = ADC_SAT;
  int vMax = 0;
  int iMax = 0;

  const uint32_t shortStartUsecs = micros();
  prepareIscPath();
  if (settleMs > 0) {
    delay(settleMs);
  }

  const uint32_t sampleStartUsecs = micros();
  for (int sample = 0; sample < sampleCount; ++sample) {
    const int current = readAdc(ADC_CURRENT_CH);
    const int voltage = readAdc(ADC_VOLTAGE_CH);
    iSum += current;
    vSum += voltage;
    iMin = min(iMin, current);
    iMax = max(iMax, current);
    vMin = min(vMin, voltage);
    vMax = max(vMax, voltage);
  }
  const uint32_t sampleUsecs = micros() - sampleStartUsecs;
  const uint32_t shortUsecs = micros() - shortStartUsecs;
  endSweepPath();

  out.print(F("ISC_TEST samples="));
  out.print(sampleCount);
  out.print(F(" settle_ms="));
  out.print(settleMs);
  out.print(F(" short_us="));
  out.print(shortUsecs);
  out.print(F(" sample_us="));
  out.println(sampleUsecs);

  out.print(F("ISC_CH0 avg="));
  out.print(static_cast<float>(vSum) / sampleCount, 2);
  out.print(F(" min="));
  out.print(vMin);
  out.print(F(" max="));
  out.println(vMax);

  out.print(F("ISC_CH1 avg="));
  out.print(static_cast<float>(iSum) / sampleCount, 2);
  out.print(F(" min="));
  out.print(iMin);
  out.print(F(" max="));
  out.println(iMax);
}

void printVocNoise(Stream &out) {
  out.print(F("VOC loops="));
  out.print(lastVocLoops);
  out.print(F(" ch0_mode="));
  out.print(lastVocAdc);
  out.print(F(" ch0_count="));
  out.print(lastVocCount);
  out.print(F(" ch1_min="));
  out.print(lastNoiseMin);
  out.print(F(" ch1_max="));
  out.println(lastNoiseMax);
}
