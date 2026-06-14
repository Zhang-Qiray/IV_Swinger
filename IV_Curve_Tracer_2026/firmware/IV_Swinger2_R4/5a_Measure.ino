/*
 * 5a_Measure.ino
 *
 * Safe VOC / ISC measurement.
 */

void measureVocForSweep() {
  setIdleState();
  delay(20);
  measureVocAverage(VOC_SAMPLE_COUNT);
}

bool measureStableIscForSweep() {
  prepareIscPath();
  delay(20);

  measureIscAverage(VOC_SAMPLE_COUNT);

  if (!lastIscStable) {
    endSweepPath();
  }

  return lastIscStable;
}

bool measureStableIscForCommand() {
  const bool stable = measureStableIscForSweep();
  endSweepPath();
  return stable;
}

void measureVocAverage(int loops) {
  lastVocLoops = constrain(loops, 1, 2000);

  long vSum = 0;

  lastVocAdc = 0;
  lastVocCount = 0;
  lastNoiseMin = ADC_SAT;
  lastNoiseMax = 0;

  for (int i = 0; i < lastVocLoops; ++i) {
    int v = readAdc(ADC_VOLTAGE_CH);
    int c = readAdc(ADC_CURRENT_CH);

    vSum += v;

    lastNoiseMin = min(lastNoiseMin, c);
    lastNoiseMax = max(lastNoiseMax, c);
  }

  lastVocAdc = vSum / lastVocLoops;
  lastVocCount = lastVocLoops;
}

void measureIscAverage(int loops) {
  const int count = constrain(loops, 1, 2000);
  const int minIscAdc = min_isc_adc + lastNoiseMin;
  const int minValidCount = 5;

  long iSum = 0;
  int validCount = 0;

  lastIscStable = false;
  lastIscAdc = 0;

  for (int i = 0; i < count; ++i) {
    int current = readAdc(ADC_CURRENT_CH);
    readAdc(ADC_VOLTAGE_CH);

    if (current > minIscAdc) {
      iSum += current;
      validCount++;
    }
  }

  if (validCount >= minValidCount) {
    lastIscAdc = iSum / validCount;
    lastIscStable = true;
  } else if (validCount > 0) {
    lastIscAdc = iSum / validCount;
  }
}
