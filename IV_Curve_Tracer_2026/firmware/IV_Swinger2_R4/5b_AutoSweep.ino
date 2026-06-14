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
      sweepReachedEnd = true;
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
  captureFormalSweepPoints(startMicros);

  endAdcBurst();
  sweepElapsedMicros = micros() - startMicros;
  endSweepPath();
}

void captureFormalSweepPoints(uint32_t startMicros) {
  uint32_t nextSaveMicros = 0;

  while (sweepRawPointCount < MAX_RAW_SWEEP_POINTS_TO_READ) {
    readLatestSweepPoint();
    sweepRawPointCount++;

    const uint32_t elapsedMicros =
        sweepSaveAllRawPoints ? 0 : micros() - startMicros;
    if (shouldSaveFormalSweepPoint(elapsedMicros, &nextSaveMicros)) {
      saveLatestSweepPoint();
    }

    if (sweepOutputCurrentReachedZero()) {
      sweepCurrentReachedZero = true;
      sweepReachedEnd = true;
      break;
    }

    if (sweepTimedOut(startMicros)) {
      break;
    }
  }
}

bool shouldSaveFormalSweepPoint(uint32_t elapsedMicros,
                                uint32_t *nextSaveMicros) {
  if (sweepOutputPointCount >= sweepOutputPointLimit) {
    return false;
  }

  if (sweepSaveAllRawPoints) {
    return true;
  }

  if (elapsedMicros < *nextSaveMicros) {
    return false;
  }

  *nextSaveMicros += sweepSaveIntervalMicros;
  return true;
}

// Prepare both endpoint measurements before a sweep starts:
//   Voc is measured for reporting and current-channel noise tracking.
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

// Stop only after more than 20 consecutive raw zero-current points.
bool sweepOutputCurrentReachedZero() {
  if (latestSweepPoint.i != 0) {
    zeroCurrentConfirmCount = 0;
    return false;
  }

  zeroCurrentConfirmCount++;
  return zeroCurrentConfirmCount >= ZERO_CURRENT_CONFIRM_POINTS;
}

void saveLatestSweepPoint() {
  scratch.rawPoints[sweepOutputPointCount] = latestSweepPoint;
  sweepOutputPointCount++;
}
