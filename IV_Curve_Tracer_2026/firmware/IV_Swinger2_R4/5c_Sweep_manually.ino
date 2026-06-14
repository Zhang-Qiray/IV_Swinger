/*
 * 5c_Sweep_manually.ino
 *
 * Manual scan. User choose point count and sample delay, then
 * compare how those settings change the IV curve.
 */

void runTeachingSweep(Stream &out, int requestedPoints, int sampleDelayMicros) {
  const int targetPoints = constrain(requestedPoints, 1, MAX_RAW_POINTS);
  const int delayMicros = constrain(sampleDelayMicros, 0, 1000);

  saveLimit = targetPoints;
  rawPointsRead = 0;
  pointsSaved = 0;
  sweepElapsedMicros = 0;
  sweepManualDelayMicros = delayMicros;
  sweepSaveAllRawPoints = true;

  measureVocForSweep();
  if (!measureStableIscForSweep()) {
    out.println(F("ERR SWEEP_T isc_not_stable"));
    out.println(F("END_SWEEP status=error"));
    return;
  }

  sweepVocAdcCount = lastVocAdc;
  sweepIscAdcCount = lastIscAdc;

  startSweepPath();
  const uint32_t startMicros = micros();
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
  sweepElapsedMicros = micros() - startMicros;
  endSweepPath();

  rawPointsRead = targetPoints;
  pointsSaved = targetPoints;
  printCleanSweepData(out);
}
