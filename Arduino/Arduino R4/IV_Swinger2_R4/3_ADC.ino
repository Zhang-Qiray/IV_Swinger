/*
 * 3_ADC.ino
 *
 * MCP3202 ADC access only.
 *
 * Main functions:
 *   adcBegin()       Initialize SPI and the CS pin
 *   readAdc(ch)      Read CH0 or CH1
 *   setAdcSpiHz(hz)  Change SPI speed
 *
 * Channel convention follows the original project:
 *   CH0 = voltage
 *   CH1 = current
 */

void adcBegin() {
  pinMode(PIN_ADC_CS, OUTPUT);
  digitalWrite(PIN_ADC_CS, HIGH);
  SPI.begin();
}

void setAdcSpiHz(uint32_t hz) {
  configSpiHz = hz;
}

int readAdc(uint8_t channel) {
  SPI.beginTransaction(SPISettings(configSpiHz, MSBFIRST, SPI_MODE0));
  const int value = readAdcInTransaction(channel);
  SPI.endTransaction();

  return value;
}

void beginAdcBurst() {
  SPI.beginTransaction(SPISettings(configSpiHz, MSBFIRST, SPI_MODE0));
}

void endAdcBurst() {
  SPI.endTransaction();
}

int readAdcInTransaction(uint8_t channel) {
  // MCP3202 command bits:
  // START=1, single-ended mode, selected channel, MSB first.
  uint8_t cmd = (channel == ADC_VOLTAGE_CH) ? B10100000 : B11100000;

  digitalWrite(PIN_ADC_CS, LOW);
  SPI.transfer(B00000001);
  uint8_t msByte = SPI.transfer(cmd) & B00001111;
  uint8_t lsByte = SPI.transfer(0x00);
  digitalWrite(PIN_ADC_CS, HIGH);

  return (static_cast<int>(msByte) << 8) | lsByte;
}
