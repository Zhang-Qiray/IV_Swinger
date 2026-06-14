/*
 * 5d_Output.ino
 *
 * Conversion and simple IV output used by SWEEP and SWEEP_T.
 */

float smartAdcToAmps(int currentAdc) {
  const float adcInc = adcReferenceVolts / ADC_MAX;
  const float opAmpGain =
      (currentAmpFeedbackOhms + currentAmpGroundOhms) / currentAmpGroundOhms;
  const float rawAmps = currentAdc * adcInc / opAmpGain / currentShuntOhms;
  return rawAmps * currentScale + currentOffset;
}

float smartAdcToVolts(int voltageAdc) {
  const float adcInc = adcReferenceVolts / ADC_MAX;
  const float vdivRatio =
      voltageDividerBottomOhms / (voltageDividerTopOhms + voltageDividerBottomOhms);
  const float rawVolts = voltageAdc * adcInc / vdivRatio;
  return rawVolts * voltageScale + voltageOffset;
}

int correctedSweepCurrentAdc(int index) {
  const int currentAdc = scratch.rawPoints[index].i;

  if (!sweepSaveAllRawPoints || index >= pointsSaved - 1) {
    return currentAdc;
  }

  const int nextCurrentAdc = scratch.rawPoints[index + 1].i;
  return (currentAdc + nextCurrentAdc + 1) / 2;
}

float averageMicrosPerOutputPoint() {
  if (pointsSaved <= 0) {
    return 0.0;
  }

  return (float)sweepElapsedMicros / pointsSaved;
}

void printMicrosPerPoint(Stream &out) {
  const float totalMicros = averageMicrosPerOutputPoint();

  out.print(F(" us/Point = "));

  if (sweepManualDelayMicros < 0) {
    out.println(totalMicros, 2);
    return;
  }

  const float adcReadMicros = max(0.0f, totalMicros - sweepManualDelayMicros);
  out.print(totalMicros, 2);
  out.print(F(" ("));
  out.print(adcReadMicros, 2);
  out.print(F(" + "));
  out.print(sweepManualDelayMicros);
  out.println(F(")"));
}

void printMaxPowerPoint(Stream &out) {
  if (pointsSaved <= 0) {
    out.println(F("MPP P = 0.0000 I = 0.0000 V = 0.0000"));
    return;
  }

  float maxPower = -1.0f;
  float maxPowerAmps = 0.0f;
  float maxPowerVolts = 0.0f;

  for (int index = 0; index < pointsSaved; ++index) {
    const RawPoint point = scratch.rawPoints[index];
    const float amps = smartAdcToAmps(correctedSweepCurrentAdc(index));
    const float volts = smartAdcToVolts(point.v);
    const float watts = volts * amps;

    if (watts > maxPower) {
      maxPower = watts;
      maxPowerAmps = amps;
      maxPowerVolts = volts;
    }
  }

  out.print(F("MPP P = "));
  out.print(maxPower, 4);
  out.print(F(" I = "));
  out.print(maxPowerAmps, 4);
  out.print(F(" V = "));
  out.println(maxPowerVolts, 4);
}

void printCleanSweepData(Stream &out) {
  const float vocVolts = smartAdcToVolts(sweepVocAdcCount);
  const float iscAmps = smartAdcToAmps(sweepIscAdcCount);

  out.print(F("Voc = "));
  out.print(vocVolts, 4);
  out.print(F(" Isc = "));
  out.print(iscAmps, 4);
  out.print(F(" Points = "));
  out.print(pointsSaved);
  printMicrosPerPoint(out);
  printMaxPowerPoint(out);

  for (int index = 0; index < pointsSaved; ++index) {
    printCleanSweepPoint(out, index);
  }
}

void printCleanSweepPoint(Stream &out, int index) {
  const RawPoint point = scratch.rawPoints[index];
  const float amps = smartAdcToAmps(correctedSweepCurrentAdc(index));
  const float volts = smartAdcToVolts(point.v);

  out.print(F("I = "));
  out.print(amps, 4);
  out.print(F(" V = "));
  out.println(volts, 4);
}
