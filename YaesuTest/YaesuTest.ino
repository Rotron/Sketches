//
// File   : YaesuCAT.ino
//
// Purpose: Test program for Yaesu CAT
//
// $Id$
//

#define DEBUG

#include <CATutil.h>
#include <YaesuCAT.h>

/** LED which indicates that TX loop is 'locked'. */
#define TX_LOCKED_LED     13

#ifdef DEBUG
 // http://arduiniana.org/libraries/streaming/
 #include <Streaming.h>
 
 // UART baud rate
 #define UART_BAUD_RATE  9600
#endif // DEBUG

#if defined (__AVR__)
/**
 * From SoftwareSerialExample:
 
   Note:
    Not all pins on the Mega and Mega 2560 support change interrupts,
    so only the following can be used for RX:
    10, 11, 12, 13, 50, 51, 52, 53, 62, 63, 64, 65, 66, 67, 68, 69

    Not all pins on the Leonardo support change interrupts,
    so only the following can be used for RX:
    8, 9, 10, 11, 14 (MISO), 15 (SCK), 16 (MOSI).
*
*/
#include <SoftwareSerial.h>

SoftwareSerial txSerial(8, 9); // RX(or), TX(rd)
#else
 // use RX3,TX3 on the Teensy 3.1/3.2 board, Serial1 is identical to Serial and wired to USB
 #define txSerial  Serial3
#endif // (__AVR__)

// local prototypes
#if defined (__AVR__)
static int availableMemory();
#endif // (__AVR__)

void setup() {

  pinMode(TX_LOCKED_LED, OUTPUT);
  digitalWrite(TX_LOCKED_LED, LOW);

#ifdef DEBUG
  /* Initialize serial output at UART_BAUD_RATE bps */
  Serial.begin(UART_BAUD_RATE);
  Serial << F("YaesuTest: starting ...") << endl;
  
 #if defined (__AVR__)
  Serial << F("Free SRAM: ") << availableMemory() << endl;
 #endif // (__AVR__)
#endif // DEBUG

  // set the data rate for the SoftwareSerial port(s)
  txSerial.begin(4800);  // menu item 14: 4800/9600/38400
}

YaesuCAT FT817(txSerial);

/** Request TX frequency and mode settings from time to time. */
static void txRequestFrequencyAndMode(YaesuCAT& ft817,unsigned long interval=200) {
  
  static unsigned long r_time = millis();
  
  if ( (millis() - r_time) > interval ) {

    ft817.requestFrequencyAndMode();
    
    r_time = millis();
  }
}

/** Set the desired frequency and mode of the TX (one quantity at a time). */
static void txSetFrequencyAndMode(YaesuCAT& ft817, 
                                  uint32_t& desired_frequency,
                                  byte& desired_mode,
                                  unsigned long interval=500) {
  
  static unsigned long s_time = millis();

  uint32_t frequency = ft817.getFrequency();
  byte mode = ft817.getMode();
  
  // send message to the rig
  if ( millis() - s_time > interval ) {
    
    s_time = millis();

    if ( frequency != desired_frequency )
      ft817.writeFrequency(desired_frequency);
    else if ( mode != desired_mode )
      ft817.writeMode(desired_mode);
  }
}

// the program's main loop
void loop() {

  uint32_t frequency = FT817.getFrequency();
  byte mode = FT817.getMode();

  static byte txState = 0;

  static unsigned long loop_init = millis();
  static unsigned long tx_init = millis();

  uint32_t desired_frequency = 28175000; // 70.175 MHz transverted into 10m Band
  byte desired_mode = YaesuCAT::eModeUSB;

  // set the rxState appropriately
  if ( frequency == YaesuCAT::ILLEGAL_FREQ || mode == YaesuCAT::ILLEGAL_MODE )
    txState = 0;
  else if ( frequency != desired_frequency || mode != desired_mode )
    txState = 1;
  else
    txState = 2;

  switch ( txState ) {

    case 0:
      txRequestFrequencyAndMode(FT817, 200);
      break;

    case 1:
      txSetFrequencyAndMode(FT817, desired_frequency, desired_mode, 500);
      txRequestFrequencyAndMode(FT817, 200);
      break;

    case 2:
      if ( (millis() - tx_init) > 5000 ) {
        FT817.requestFrequencyAndMode();
        tx_init = millis();
      }
      break;

    default:
      ;
  }

  if ( txState == 2 )
    digitalWrite(TX_LOCKED_LED, HIGH);
  else
    digitalWrite(TX_LOCKED_LED, LOW);

#if defined (__AVR__)
  txSerial.listen();
#endif // (__AVR__)

  // check if 'txSerial' has data to receive
  if (txSerial.available()) {

    FT817.read();

  } // txSerial.available()

  // display of frequency and mode from time to time
  if ( millis() - loop_init > 2000 ) {
    
    loop_init = millis();

    Serial << F("txState ") << txState 
           //<< F(" fOK ") << rxFrequencyOK(Icom706)
           //<< F(" mOK ") << rxModeOK(Icom706)
           << F(": Frequency = ") << frequency 
           << F(" Mode = ") << mode << endl;
  }
}


/** 
 *  Try to inquire max. available memory by allocating until heap is exhausted.
 */
#if defined (__AVR__)
static int availableMemory() {

#if 0
  // from: https://devel-ik.fzk.de/wsvn/fd-online/Diverse/Oktokopter/Arduino2Flasher/trunk/Arduino2Flasher.ino
  int size = 10000;
  byte *buf;
  while ((buf = (byte *) malloc(--size)) == NULL);
  free(buf);
  return size;
#else
  // see also:
  // https://learn.adafruit.com/memories-of-an-arduino/measuring-free-memory
  extern int __heap_start, *__brkval; 
  int v; 

  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
#endif
}
#endif // (__AVR__)

