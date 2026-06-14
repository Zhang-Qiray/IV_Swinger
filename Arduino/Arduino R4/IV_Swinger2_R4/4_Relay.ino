/*
 * 4_Relay.ino
 *
 * SSR1/SSR2/SSR3 control only.
 * Other files should not digitalWrite relay pins directly; call these helpers.
 *
 * The three high-level state helpers are the important ones:
 *   setIdleState()    Safe idle/discharge state
 *   prepareIscPath()  Build the Isc path before a sweep
 *   startSweepPath()  Open the Isc bypass and start the real sweep
 */

// Track logical relay state for STATE output and debugging.
bool relay1Active = false;
bool relay2Active = false;
bool relay3Active = false;

void relaysBegin() {
  pinMode(PIN_SSR1, OUTPUT);
  pinMode(PIN_SSR2, OUTPUT);
  pinMode(PIN_SSR3, OUTPUT);
  setIdleState();
}

void setIdleState() {
  // Safe idle state, also the default state before and after tests:
  // SSR1 off: PV is not connected to the load capacitor
  // SSR2 on : load capacitor discharges through the bleed path
  // SSR3 off: Isc bypass is open
  setSsr1(false);
  setSsr2(true);
  setSsr3(false);
}

void prepareIscPath() {
  // Build the short-circuit current path before the sweep.
  // Order matters: enable SSR3 bypass, enable SSR1, then disable SSR2 bleed.
  setSsr3(true);
  delay(20);
  setSsr1(true);
  setSsr2(false);
}

void startSweepPath() {
  // Opening SSR3 lets PV current charge the load capacitor and start the I-V sweep.
  setSsr3(false);
}

void endSweepPath() {
  // Return to the safe discharge state after a sweep.
  setIdleState();
}

void setSsr1(bool active) {
  // SSR1 is fixed active-low: true = PV connected to the load capacitor.
  relay1Active = active;
  digitalWrite(PIN_SSR1, active ? LOW : HIGH);
}

void setSsr2(bool active) {
  // SSR2 is fixed active-high: true = bleed path enabled.
  relay2Active = active;
  digitalWrite(PIN_SSR2, active ? HIGH : LOW);
}

void setSsr3(bool active) {
  // SSR3 is fixed active-low: true = Isc bypass enabled.
  relay3Active = active;
  digitalWrite(PIN_SSR3, active ? LOW : HIGH);
}

void printRelayState(Stream &out) {
  out.print(F("RELAY_STATE ssr1="));
  out.print(relay1Active ? F("ON") : F("OFF"));
  out.print(F(" ssr2="));
  out.print(relay2Active ? F("ON") : F("OFF"));
  out.print(F(" ssr3="));
  out.println(relay3Active ? F("ON") : F("OFF"));
}
