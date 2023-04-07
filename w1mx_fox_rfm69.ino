// TxPowerTest_Transmitter for RFM69 transceiver radios
// This turns on the Transmitter (unmodulated carrier) continuously
// Should be used experimentally to measure power output and current of the transmitter
// Use on a frequency that does not interfere with any other known active frequencies
// Ensure settings match with the Receiver sketch if you use that to measure the RSSI
// Trasmitter is toggled with 't' (or tactile/SPST button that pulls A0 to GND)
// Transmitter power is controlled with +,- in steps, or <,> in dBm
// **********************************************************************************
// Copyright Felix Rusu 2021, http://www.LowPowerLab.com/contact
// **********************************************************************************
#include <RFM69.h>      //get it here: https://www.github.com/lowpowerlab/rfm69
#include <RFM69registers.h>


#define FREQUENCY     RF69_433MHZ
#define FREQUENCY_EXACT 433930000
#define IS_RFM69HW_HCW   //uncomment only for RFM69HCW! Leave out if you have RFM69CW!

#define FXOSC 32000000

#define SERIAL_BAUD   500000
#define DEBUG(input)   Serial.print(input)
#define DEBUGln(input) Serial.println(input)
#define DEBUGHEX(input, param) Serial.print(input, param)

#define PIN_RFM69_RESET 3
#define PIN_POWER_LED 2

RFM69 radio;

#define SIDETONE_HZ 800

void setRFbitrate(uint32_t hz) {
  uint16_t rfbitrate = FXOSC/hz/2;

  radio.writeReg(REG_BITRATEMSB, rfbitrate>>8);
  radio.writeReg(REG_BITRATELSB, rfbitrate);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_POWER_LED, OUTPUT);
  digitalWrite(PIN_POWER_LED, HIGH);
  pinMode(PIN_RFM69_RESET, OUTPUT);
  digitalWrite(PIN_RFM69_RESET, HIGH);
  delayMicroseconds(200);
  digitalWrite(PIN_RFM69_RESET, LOW);
  delay(5);

  DEBUGln("START RFM69_NODE_TX_TEST!");

  if (!radio.initialize(FREQUENCY,0,0))
    DEBUGln("radio.init() FAIL");
  else
    DEBUGln("radio.init() SUCCESS");

#ifdef IS_RFM69HW_HCW
  radio.setHighPower(); //only for RFM69HW/HCW!
#endif

#ifdef FREQUENCY_EXACT
  radio.setFrequency(FREQUENCY_EXACT); //set frequency to some custom frequency
#endif

  uint16_t fdev = 2500/RF69_FSTEP;
  radio.writeReg(REG_FDEVMSB, fdev>>8);
  radio.writeReg(REG_FDEVLSB, fdev);

  setRFbitrate(SIDETONE_HZ);

  // No preamble
  radio.writeReg(REG_PREAMBLEMSB, 0);
  radio.writeReg(REG_PREAMBLELSB, 0);

  // Enable FSK mode with shaping to produce a modulated ~sine wave
  radio.writeReg(REG_DATAMODUL, RF_DATAMODUL_DATAMODE_PACKET | RF_DATAMODUL_MODULATIONTYPE_FSK | RF_DATAMODUL_MODULATIONSHAPING_11);

  // Fixed with length 0 = unlimited
  // DCFREE_MANCHESTER would start doing manchester encoding
  radio.writeReg(REG_PACKETCONFIG1, RF_PACKET1_FORMAT_FIXED | RF_PACKET1_DCFREE_OFF | RF_PACKET1_CRC_ON | RF_PACKET1_CRCAUTOCLEAR_ON | RF_PACKET1_ADRSFILTERING_OFF);
  radio.writeReg(REG_PAYLOADLENGTH, 0);
  radio.writeReg(REG_FIFO, 0);

  radio.writeReg(REG_SYNCCONFIG, RF_SYNC_OFF | RF_SYNC_FIFOFILL_AUTO | RF_SYNC_SIZE_2 | RF_SYNC_TOL_0);
  radio.writeReg(REG_SYNCVALUE1, 0);
  radio.writeReg(REG_SYNCVALUE2, 0);

  char buff[50];
  sprintf(buff, "\nTransmitting at %lu kHz...", radio.getFrequency()/1000L);
  DEBUGln(buff);

  DEBUGln("Use:\n+,- to adjust power in _powerLevel steps");
  DEBUGln("<,> to adjust power in dBm");

  radio.setMode(RF69_MODE_SLEEP);
}

void key(bool keyed) {
  radio.writeReg(REG_PACKETCONFIG1, RF_PACKET1_FORMAT_FIXED | (keyed ? RF_PACKET1_DCFREE_MANCHESTER : RF_PACKET1_DCFREE_OFF) | RF_PACKET1_CRC_ON | RF_PACKET1_CRCAUTOCLEAR_ON | RF_PACKET1_ADRSFILTERING_OFF);
}

const char* alphabet = "A.-B-...C-.-.D-..E.F..-.G--.H....I..J.---K-.-L.-..M--N-.O---P.--.Q--.-R.-.S...T-U..-V...-W.--X-..-Y-.--Z--.."
  "1.----2..---3...--4....-5.....6-....7--...8---..9----.0-----";

//Parameter which indirectly sets morsing speed. Higher = slower. Defined in milliseconds.
//Rule of thumb I derived: 1250 / this number = WPM.
int dotDuration = 75;
//This message is what will be transmitted in Morse.
String programMessage = "W1XM FOX HUNT AT MIT";
//Amount of time to wait between transmissions. In seconds.
unsigned long transmitDelay = 60;


//Takes a character and will return symbols. Example: getMorse("A") returns ".-"
const char* getMorse(char letter)
{
  if (letter >= 'a') {
    letter -= ('a' - 'A');
  }
  char* p = strchr(alphabet, letter);
  if (p)
    return p+1;
  return NULL;
}

void txOne(bool tone, uint32_t ms, uint16_t freq) {
  uint32_t cycles = ms / (4000 / freq);
  DEBUG(ms);
  DEBUG("ms = ");
  DEBUG(cycles);
  DEBUG(" cycles of ");
  DEBUGln(tone ? freq : 0);
  while (cycles--) {
    while (radio.readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_FIFOFULL);
    radio.writeReg(REG_FIFO, tone ? 0x55 : 0);
  }
}

void tx(bool tone, uint32_t ms, uint32_t gap_ms = 0, uint16_t freq = SIDETONE_HZ) {
  if (freq != SIDETONE_HZ) {
      while (radio.readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_FIFONOTEMPTY);
      setRFbitrate(freq);
  }
  digitalWrite(PIN_POWER_LED, !tone);
  txOne(tone, ms, freq);
  digitalWrite(PIN_POWER_LED, HIGH);
  if (gap_ms)
    txOne(false, gap_ms, freq);
  if (freq != SIDETONE_HZ) {
      while (radio.readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_FIFONOTEMPTY);
      setRFbitrate(SIDETONE_HZ);
  }
}

int transmit_count = 0;

void transmit(String message)
{
  // Preload with a few 0s
  for (int i = 0; i < 15; i++)
    radio.writeReg(REG_FIFO, 0);
  radio.setMode(RF69_MODE_TX);
  message.toUpperCase();
  tx(false, 500);
  //Transmit each letter of the message
  for(int i=0; i<message.length(); i++)
  {
    char letter = message[i];
    if(letter == ' ')
      //Specified delay between words is 7. Down below we pause for 3 so this is the other 4.
      tx(false, dotDuration*4);
    const char* p = getMorse(letter);
    //Transmit each symbol of the current letter
    while (p && *p) {
      switch (*(p++)) {
        case '.':
          tx(true, 1*dotDuration);
          break;
        case '-':
          tx(true, 3*dotDuration);
          break;
        default:
          p = NULL;
      }
      tx(false, dotDuration);
    }
   tx(false, dotDuration*3);
  }
  tx(false, 1000);
  tx(true, 10000);
  tx(false, 1000);
  while ((radio.readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PACKETSENT) == 0x00);
  radio.setMode(RF69_MODE_SLEEP);
}

//Tracks the last time we transmitted, so we can do that automatically.
unsigned long lastTransmissionTime = 0;

int8_t dBm=-18; //start at minimum possible value for W/CW, gets bumped by library to -2 for HW/HCW

void loop() {
  if (Serial.available() > 0) {
    char input = Serial.read();

    if (input == 'r') //d=dump register values
      radio.readAllRegsCompact();

    if (input == 'R') //d=dump register values
      radio.readAllRegs();

    if (input == ',')
      key(false);
    if (input == '.')
      key(true);

    if (input=='+') {
      radio.setPowerLevel(radio.getPowerLevel()+1);
      DEBUG("_powerLevel=");DEBUGln(radio.getPowerLevel());
    }
    if (input=='-') {
      if (radio.getPowerLevel()>0) {
        radio.setPowerLevel(radio.getPowerLevel()-1);
      }
      DEBUG("_powerLevel=");DEBUGln(radio.getPowerLevel());
    }
    if (input=='<') {
      dBm = radio.setPowerDBm(dBm-1);
      DEBUG("POWER=");DEBUG(dBm);DEBUG(" (dBm); _powerLevel=");DEBUGln(radio.getPowerLevel());
    }
    if (input=='>') {
      dBm = radio.setPowerDBm(dBm+1);
      DEBUG("POWER=");DEBUG(dBm);DEBUG(" (dBm); _powerLevel=");DEBUGln(radio.getPowerLevel());
    }

    if (input=='T') {
      transmit("AB1IZ TEST");
    }
    if (input==',') {
      radio.setMode(RF69_MODE_SLEEP);
    }
    if (input=='.') {
      radio.setMode(RF69_MODE_TX);
    }
  }
  if (!lastTransmissionTime || millis() > (lastTransmissionTime + 1000*transmitDelay)) {
    transmit("W1MX FOX HUNT AT MIT");
    lastTransmissionTime = millis();
  }
}
