/*
 * 2_Config.ino
 *
 * Central firmware configuration.
 *
 * This file intentionally keeps all hardware mapping and tunable runtime
 * values together:
 *   1. UNO R4 pin assignments
 *   2. ADC / SPI channel settings
 *   3. Voltage and current conversion constants
 *   4. Shared sweep buffers and last-measurement state
 *
 * Hardware-control files should use these constants, not hard-coded pin
 * numbers or ADC channel numbers.
 */

// ---------------------------------------------------------------------------
// UNO R4 hardware pin map
// ---------------------------------------------------------------------------

// D0/D1 are the USB serial pins. They are listed here so the reserved pins are
// obvious when checking wiring, but the firmware does not configure them.
const uint8_t PIN_USB_RX = 0;       // D0, USB serial RX, reserved
const uint8_t PIN_USB_TX = 1;       // D1, USB serial TX, reserved

// IV Swinger 2 SSR module control outputs.
const uint8_t PIN_SSR1 = 2;         // Main PV-to-capacitor sweep switch
const uint8_t PIN_SSR2 = 6;         // Capacitor bleed/discharge path
const uint8_t PIN_SSR3 = 7;         // Isc bypass around the load capacitors

// MCP3202 ADC chip-select. MOSI/MISO/SCK use the UNO R4 hardware SPI pins.
const uint8_t PIN_ADC_CS = 10;

// ---------------------------------------------------------------------------
// ADC / SPI configuration
// ---------------------------------------------------------------------------

// ADC / SPI
const uint8_t ADC_VOLTAGE_CH = 0;   // MCP3202 CH0 measures panel voltage
const uint8_t ADC_CURRENT_CH = 1;   // MCP3202 CH1 measures shunt current
const int ADC_MAX = 4096;           // 12-bit ADC count range is 0..4095
const int ADC_SAT = 4095;           // Saturated 12-bit ADC reading
uint32_t configSpiHz = 4000000;     // Runtime SPI clock, adjustable by SPI_HZ

// ---------------------------------------------------------------------------
// ADC-count to real-unit conversion constants
// ---------------------------------------------------------------------------

// Hardware defaults. CAL prints them, and SET_CAL can temporarily adjust them
// at runtime for calibration tests. Resetting the board restores these values.
float adcReferenceVolts = 4.880; 
float voltageDividerTopOhms = 150000.0;  //150k
float voltageDividerBottomOhms = 7500.0;  //7.5k
float currentAmpFeedbackOhms = 75000.0;   //75k
float currentAmpGroundOhms = 1000.0;
float currentShuntOhms = 0.005;      //0.005ohm

// Final linear calibration:
//   displayed_value = raw_converted_value * scale + offset
float voltageScale = 1.0;
float voltageOffset = 0.0;
float currentScale = 1.0;
float currentOffset = 0.0;

// ---------------------------------------------------------------------------
// Isc stability check
// ---------------------------------------------------------------------------

// A short-circuit current sample is considered useful only when it rises above
// the measured current-channel noise floor by at least min_isc_adc counts.
int min_isc_adc = 100;
int max_isc_poll = 5000;
int isc_stable_adc = 5;

// ---------------------------------------------------------------------------
// Shared sweep data
// ---------------------------------------------------------------------------

struct RawPoint {
  uint16_t v;
  uint16_t i;
};

// Sweep output buffer. Normal automatic sweeps target 2500 plotted points, but
// the buffer keeps 100 extra points in reserve in case the formal sweep runs a
// little slower than the prescan.
const int MAX_RAW_POINTS = 2600;

struct ScratchBuffer {
  RawPoint rawPoints[MAX_RAW_POINTS];
};

ScratchBuffer scratch;

int lastVocLoops = 0;
int lastVocAdc = 0;
uint16_t lastVocCount = 0;
int lastNoiseMin = ADC_SAT;
int lastNoiseMax = 0;

bool lastIscStable = false;
int lastIscAdc = 0;

// ---------------------------------------------------------------------------
// Runtime calibration command helpers
// ---------------------------------------------------------------------------

bool setCalibrationValue(const char *name, float value) {
  if (equalsIgnoreCase(name, "adc_ref")) {
    adcReferenceVolts = value;
  } else if (equalsIgnoreCase(name, "v_top")) {
    voltageDividerTopOhms = value;
  } else if (equalsIgnoreCase(name, "v_bottom")) {
    voltageDividerBottomOhms = value;
  } else if (equalsIgnoreCase(name, "i_feedback")) {
    currentAmpFeedbackOhms = value;
  } else if (equalsIgnoreCase(name, "i_ground")) {
    currentAmpGroundOhms = value;
  } else if (equalsIgnoreCase(name, "i_shunt")) {
    currentShuntOhms = value;
  } else if (equalsIgnoreCase(name, "v_scale")) {
    voltageScale = value;
  } else if (equalsIgnoreCase(name, "v_offset")) {
    voltageOffset = value;
  } else if (equalsIgnoreCase(name, "i_scale")) {
    currentScale = value;
  } else if (equalsIgnoreCase(name, "i_offset")) {
    currentOffset = value;
  } else {
    return false;
  }
  return true;
}

void printCalibrationNames(Stream &out) {
  out.println(F("CAL_NAMES adc_ref v_top v_bottom i_feedback i_ground i_shunt v_scale v_offset i_scale i_offset"));
}

void printCalibration(Stream &out) {
  out.print(F("CAL adc_ref="));
  out.print(adcReferenceVolts, 6);
  out.print(F(" v_top="));
  out.print(voltageDividerTopOhms, 2);
  out.print(F(" v_bottom="));
  out.print(voltageDividerBottomOhms, 2);
  out.print(F(" i_feedback="));
  out.print(currentAmpFeedbackOhms, 2);
  out.print(F(" i_ground="));
  out.print(currentAmpGroundOhms, 2);
  out.print(F(" i_shunt="));
  out.print(currentShuntOhms, 6);
  out.print(F(" v_scale="));
  out.print(voltageScale, 6);
  out.print(F(" v_offset="));
  out.print(voltageOffset, 6);
  out.print(F(" i_scale="));
  out.print(currentScale, 6);
  out.print(F(" i_offset="));
  out.println(currentOffset, 6);
}
