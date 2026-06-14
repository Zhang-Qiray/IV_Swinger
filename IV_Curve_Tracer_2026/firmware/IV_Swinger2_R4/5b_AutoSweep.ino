/*  
 * 5b_AutoSweep.ino
 *
 * Automatic SWEEP:
 *   1. Prescan as fast as possible to learn the physical sweep duration.
 *   2. Capture again, either saving every raw point or saving by time interval.
 *   3. Print a clean IV curve.
 */

void runAutomaticSweep(Stream &out, int requestedOutputPoints, bool verboseOutput) {
  sweepOutputPointLimit =
      min(requestedOutputPoints + SWEEP_OUTPUT_POINT_RESERVE, MAX_RAW_POINTS);
  sweepSaveIntervalMicros = 0;
  sweepSaveAllRawPoints = false;
  sweepManualDelayMicros = -1;

  if (verboseOutput) {
    out.println(F("OK SWEEP_ALL begin"));
    out.println(F("SWEEP_METHOD auto_prescan_save_all_or_time_interval"));
  }

  runPrescanSweep();
  if (!automaticPrescanSucceeded(out, verboseOutput)) {
    return;
  }

  chooseOutputSaveStrategy(requestedOutputPoints);

  setIdleState();
  delay(80);

  runFormalSweep();
  if (!formalSweepSucceeded(out, verboseOutput)) {
    return;
  }

  if (verboseOutput) {
    printSweepAllSummary(out);
    out.println(F("END_SWEEP_ALL status=ok"));
  } else {
    printCleanSweepData(out);
  }
}

// The prescan is allowed to reject the sweep before we spend time on the real
// capture. That keeps the normal SWEEP output clean: either useful IV points or
// a short error message.
bool automaticPrescanSucceeded(Stream &out, bool verboseOutput) {
  if (verboseOutput) {
    out.println(F("PRESCAN"));
    printSweepPassSummary(out, "  ");
  }

  if (!sweepIscReady) {
    out.println(F("ERR SWEEP prescan_isc_not_stable"));
    out.println(F("END_SWEEP status=error"));
    return false;
  }

  if (!sweepReachedEnd) {
    out.println(F("ERR SWEEP prescan_incomplete"));
    out.println(F("END_SWEEP status=error"));
    return false;
  }

  return true;
}

// Use the requested point count to choose the normal spacing, but allow the
// reserve buffer to absorb a slightly slower formal sweep.
void chooseOutputSaveStrategy(int requestedOutputPoints) {
  sweepSaveIntervalMicros =
      max(1UL, sweepElapsedMicros / max(1, requestedOutputPoints - 1));
  sweepSaveAllRawPoints = sweepRawPointCount <= requestedOutputPoints;
}

// A formal sweep with a valid Isc is useful even if it times out before the
// ideal end condition. In verbose mode we report that as a warning.
bool formalSweepSucceeded(Stream &out, bool verboseOutput) {
  if (!sweepIscReady) {
    if (verboseOutput) {
      printSweepAllSummary(out);
    }
    out.println(F("ERR SWEEP formal_isc_not_stable"));
    out.println(F("END_SWEEP status=error"));
    return false;
  }

  if (!verboseOutput) {
    return true;
  }

  if (!sweepReachedEnd) {
    out.println(F("WARN SWEEP incomplete"));
  }

  return true;
}

// First pass: measure how long this panel/capacitor sweep really takes.
void runPrescanSweep() {
  preparePanelForSweep();
  if (!sweepIscReady) {
    return;
  }

  startSweepPath();
  const uint32_t startMicros = micros();
  beginAdcBurst();

  while (sweepRawPointCount < MAX_RAW_SWEEP_POINTS_TO_READ) {
    readLatestSweepPoint();
    sweepRawPointCount++;

    if (sweepOutputCurrentReachedZero()) {
      sweepCurrentReachedZero = true;
      sweepReachedEnd = sweepVoltageReachedVocPercent();
      break;
    }

    if (micros() - startMicros >= MAX_SWEEP_ELAPSED_MICROS) {
      break;
    }
  }

  endAdcBurst();
  sweepElapsedMicros = micros() - startMicros;
  endSweepPath();
}

// Second pass: capture points for output using the save strategy chosen from
// the prescan.
void runFormalSweep() {
  preparePanelForSweep();
  if (!sweepIscReady) {
    return;
  }

  startSweepPath();
  const uint32_t startMicros = micros();
  beginAdcBurst();

  if (sweepSaveAllRawPoints) {
    saveEveryRawPointDuringFormalSweep(startMicros);
  } else {
    savePointsByTimeDuringFormalSweep(startMicros);
  }

  endAdcBurst();
  sweepElapsedMicros = micros() - startMicros;
  endSweepPath();
}

// Fast panels may produce fewer than 2500 raw points. In that case every real
// point is valuable, so save them all.
void saveEveryRawPointDuringFormalSweep(uint32_t startMicros) {
  while (sweepRawPointCount < MAX_RAW_SWEEP_POINTS_TO_READ) {
    readLatestSweepPoint();
    sweepRawPointCount++;

    if (sweepOutputPointCount < sweepOutputPointLimit) {
      saveLatestSweepPoint();
    }

    if (sweepOutputCurrentReachedZero()) {
      sweepCurrentReachedZero = true;
      sweepReachedEnd = sweepVoltageReachedVocPercent();
      break;
    }

    if (sweepTimedOut(startMicros)) {
      break;
    }
  }
}

// Slow sweeps can produce more raw points than the output buffer can hold.
// Save by time so the printed curve still spans the full sweep.
void savePointsByTimeDuringFormalSweep(uint32_t startMicros) {
  uint32_t nextSaveMicros = 0;

  while (sweepRawPointCount < MAX_RAW_SWEEP_POINTS_TO_READ) {
    readLatestSweepPoint();
    sweepRawPointCount++;

    const uint32_t elapsedMicros = micros() - startMicros;
    const bool timeToSave = elapsedMicros >= nextSaveMicros;

    if (timeToSave && sweepOutputPointCount < sweepOutputPointLimit) {
      saveLatestSweepPoint();
      nextSaveMicros += sweepSaveIntervalMicros;
    }

    if (sweepOutputCurrentReachedZero()) {
      sweepCurrentReachedZero = true;
      sweepReachedEnd = sweepVoltageReachedVocPercent();
      break;
    }

    if ((sweepRawPointCount % SWEEP_TIMEOUT_CHECK_EVERY_POINTS == 0) &&
        elapsedMicros >= MAX_SWEEP_ELAPSED_MICROS) {
      break;
    }
  }
}

// Prepare both endpoint measurements before a sweep starts:
//   Voc establishes the voltage end target and current noise floor.
//   Isc confirms the short-circuit path is working.
void preparePanelForSweep() {
  sweepRawPointCount = 0;
  sweepOutputPointCount = 0;
  zeroCurrentConfirmCount = 0;
  sweepReachedEnd = false;
  sweepCurrentReachedZero = false;
  sweepIscReady = false;
  sweepElapsedMicros = 0;

  measureVocForSweep();
  sweepVocAdcCount = lastVocAdc;
  sweepEndCurrentAdcThreshold = max(lastNoiseMin * 2, 20);

  sweepIscReady = measureStableIscForSweep();
  sweepIscAdcCount = lastIscAdc;
}

void readLatestSweepPoint() {
  latestSweepPoint.i = readAdcInTransaction(ADC_CURRENT_CH);
  latestSweepPoint.v = readAdcInTransaction(ADC_VOLTAGE_CH);
}

bool sweepTimedOut(uint32_t startMicros) {
  if (sweepRawPointCount % SWEEP_TIMEOUT_CHECK_EVERY_POINTS != 0) {
    return false;
  }

  if (micros() - startMicros < MAX_SWEEP_ELAPSED_MICROS) {
    return false;
  }

  return true;
}

// Stop only after many consecutive low-current points. This treats "output
// current has reached zero" as the physical end of the capacitor-charging sweep,
// while filtering out brief current-channel noise dips.
bool sweepOutputCurrentReachedZero() {
  if (latestSweepPoint.i > sweepEndCurrentAdcThreshold) {
    zeroCurrentConfirmCount = 0;
    return false;
  }

  zeroCurrentConfirmCount++;
  return zeroCurrentConfirmCount >= ZERO_CURRENT_CONFIRM_POINTS;
}

// After the current has reached zero, check whether the final voltage is close
// enough to the starting Voc. This decides if the sweep captured the full curve.
bool sweepVoltageReachedVocPercent() {
  const int completeVocAdc = (sweepVocAdcCount * SWEEP_END_VOC_PERCENT) / 100;
  return latestSweepPoint.v >= completeVocAdc;
}

void saveLatestSweepPoint() {
  scratch.rawPoints[sweepOutputPointCount] = latestSweepPoint;
  sweepOutputPointCount++;
}
