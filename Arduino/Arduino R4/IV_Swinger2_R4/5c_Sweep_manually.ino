/*
 * 5c_Sweep_manually.ino
 *
 * Manual scan. User choose point count and sample delay, then
 * compare how those settings change the IV curve.
 */

void runTeachingSweep(Stream &out, int requestedPoints, int sampleDelayMicros) {
  if (!captureTeachingSweep(out, requestedPoints, sampleDelayMicros)) {
    out.println(F("END_SWEEP status=error"));
    return;
  }

  sweepVocAdcCount = lastVocAdc;
  sweepIscAdcCount = lastIscAdc;
  sweepSaveAllRawPoints = true;

  printCleanSweepData(out);
}

bool captureTeachingSweep(Stream &out, int requestedPoints, int sampleDelayMicros) {
  const int targetPoints = constrain(requestedPoints, 1, MAX_RAW_POINTS);
  const int delayMicros = constrain(sampleDelayMicros, 0, 1000);
  sweepOutputPointLimit = targetPoints;
  sweepRawPointCount = 0;
  sweepOutputPointCount = 0;
  sweepElapsedMicros = 0;
  sweepManualDelayMicros = delayMicros;

  measureVocForSweep();
  if (!measureStableIscForSweep()) {
    out.println(F("ERR SWEEP_T isc_not_stable"));
    return false;
  }

  startSweepPath();
  const uint32_t startMicros = micros();
  captureFixedTeachingPoints(targetPoints, delayMicros);
  sweepElapsedMicros = micros() - startMicros;
  endSweepPath();

  sweepRawPointCount = targetPoints;
  sweepOutputPointCount = targetPoints;
  return true;
}

void captureFixedTeachingPoints(int targetPoints, int delayMicros) {
  beginAdcBurst();

  for (int index = 0; index < targetPoints; ++index) {
    const uint16_t current = readAdcInTransaction(ADC_CURRENT_CH);
    const uint16_t voltage = readAdcInTransaction(ADC_VOLTAGE_CH);
    scratch.rawPoints[index].v = voltage;
    scratch.rawPoints[index].i = current;

    if (delayMicros > 0) {
      delayMicroseconds(delayMicros);
    }
  }

  endAdcBurst();
}
