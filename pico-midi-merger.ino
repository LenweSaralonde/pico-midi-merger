/*
 * 8-port MIDI Merger for Raspberry Pi Pico
 */

#include <MIDI.h>

// Using UART0 TX for MIDI Out

// MIDI input GPIO pins numbers
#define GP_IN_0 2  // Physical pin 4
#define GP_IN_1 3  // Physical pin 5
#define GP_IN_2 4  // Physical pin 6
#define GP_IN_3 5  // Physical pin 7
#define GP_IN_4 6  // Physical pin 9
#define GP_IN_5 7  // Physical pin 10
#define GP_IN_6 8  // Physical pin 11
#define GP_IN_7 9  // Physical pin 12

// FIFO size
#define FIFO_SIZE 256

// Active sensing timeout in ms
#define ACTIVE_SENSING_TIMEOUT 5000

// Activity LED durations for MIDI out in microseconds
#define LED_BLINK_DURATION_CHANNEL 960
#define LED_BLINK_DURATION_1_BYTE 320
#define LED_BLINK_DURATION_2_BYTE 640
#define LED_BLINK_DURATION_3_BYTE 960
#define LED_BLINK_DURATION_ACTIVE_SENSING 50
#define LED_BLINK_DURATION_CLOCK 50

// Number of input ports
#define NUM_INPUT_PORTS 8

// Undefined port number
#define PORT_NONE 255

// Create input serial ports
SerialPIO SerialPIO0(SerialPIO::NOPIN, GP_IN_0, FIFO_SIZE);
SerialPIO SerialPIO1(SerialPIO::NOPIN, GP_IN_1, FIFO_SIZE);
SerialPIO SerialPIO2(SerialPIO::NOPIN, GP_IN_2, FIFO_SIZE);
SerialPIO SerialPIO3(SerialPIO::NOPIN, GP_IN_3, FIFO_SIZE);
SerialPIO SerialPIO4(SerialPIO::NOPIN, GP_IN_4, FIFO_SIZE);
SerialPIO SerialPIO5(SerialPIO::NOPIN, GP_IN_5, FIFO_SIZE);
SerialPIO SerialPIO6(SerialPIO::NOPIN, GP_IN_6, FIFO_SIZE);
SerialPIO SerialPIO7(SerialPIO::NOPIN, GP_IN_7, FIFO_SIZE);

// Create MIDI instances
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO0, MIDI0);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO1, MIDI1);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO2, MIDI2);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO3, MIDI3);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO4, MIDI4);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO5, MIDI5);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO6, MIDI6);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO7, MIDI7);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDIOUT);

// Time in microseconds the activity LED should turn off
unsigned long ledOffTime;

// Master clock port number (0-7 or PORT_NONE)
uint8_t masterClockPort = PORT_NONE;

// Master active sensing port number (0-7 or PORT_NONE)
uint8_t activeSensingPort = PORT_NONE;

// Last time in ms the last active sensing message was received
unsigned long lastActiveSensingTime;

// Keep track of the song position for each port
bool hasSongPosition[NUM_INPUT_PORTS] = {};
uint16_t songPosition[NUM_INPUT_PORTS] = {};

/**
 * Register MIDI activity for the activity LED.
 * @param ledBlinkDuration Duration in microseconds to keep the activity LED on
 */
void registerMidiOutActivity(unsigned int ledBlinkDuration) {
  digitalWrite(LED_BUILTIN, HIGH);
  ledOffTime = max(ledOffTime, micros() + ledBlinkDuration);
}

/**
 * Time out the master active sensing port if no AS message has been received during the timeout period.
 */
void handleActiveSensingTimeout() {
  unsigned long now = millis();
  if (activeSensingPort != PORT_NONE && lastActiveSensingTime + ACTIVE_SENSING_TIMEOUT < now) {
    activeSensingPort = PORT_NONE;
  } else if (now < lastActiveSensingTime) {  // The millis() counter has reset
    lastActiveSensingTime = 0;
  }
}

/**
 * Turn the activity LED off if the delay has expired.
 */
void handleLedOff() {
  unsigned long now = micros();
  if (now >= ledOffTime) {
    digitalWrite(LED_BUILTIN, LOW);
  } else if (ledOffTime - now > 1000000) {  // The micros() counter has reset
    ledOffTime = 0;
    digitalWrite(LED_BUILTIN, LOW);
  }
}

/**
 * Read and forward incoming MIDI messages from midiInInstance to MIDIOUT.
 * Based on the MidiInterface::thruFilter() function.
 * @param midiIn
 * @param portNumber
 */
template<class Transport, class Settings, class Platform>
void mergeMIDI(midi::MidiInterface<Transport, Settings, Platform>& midiIn, uint8_t portNumber) {
  // Nothing to read here
  if (!midiIn.read())
    return;

  // Channel messages
  if (midiIn.getType() >= midi::NoteOff && midiIn.getType() <= midi::PitchBend) {
    registerMidiOutActivity(LED_BLINK_DURATION_CHANNEL);
    MIDIOUT.send(midiIn.getType(), midiIn.getData1(), midiIn.getData2(), midiIn.getChannel());
  } else {
    uint16_t twoByteValue;

    // Other messages
    switch (midiIn.getType()) {
        // Real Time and 1 byte
      case midi::Start:
      case midi::Stop:
      case midi::Continue:
      case midi::SystemReset:
      case midi::TuneRequest:
        // The most recent input to receive a START command will become the clock master.
        if (midiIn.getType() == midi::Start) {
          masterClockPort = portNumber;
        }
        // The most recent input to receive a CONTINUE after a Song Position Pointer = 0
        // will become the clock master.
        else if (midiIn.getType() == midi::Continue && hasSongPosition[portNumber] && songPosition[portNumber] == 0) {
          masterClockPort = portNumber;
        }
        // Forward the real time message
        registerMidiOutActivity(LED_BLINK_DURATION_1_BYTE);
        MIDIOUT.sendRealTime(midiIn.getType());
        break;

      case midi::ActiveSensing:
        // Set the master active sensing port if none is already set
        if (activeSensingPort == PORT_NONE) {
          activeSensingPort = portNumber;
        }

        // Forward active sensing messages from the master port only
        if (portNumber == activeSensingPort) {
          lastActiveSensingTime = millis();
          registerMidiOutActivity(LED_BLINK_DURATION_ACTIVE_SENSING);
          MIDIOUT.sendActiveSensing();
        }

        break;

      case midi::Clock:
        // Forward MIDI clock from the master clock port
        // or forward all clock messages from all inputs if no master is set
        if (portNumber == masterClockPort || masterClockPort == PORT_NONE) {
          registerMidiOutActivity(LED_BLINK_DURATION_CLOCK);
          MIDIOUT.sendClock();
        }
        break;

      case midi::SystemExclusive:
        // Send SysEx (0xf0 and 0xf7 are included in the buffer)
        registerMidiOutActivity(midiIn.getSysExArrayLength() * LED_BLINK_DURATION_1_BYTE);
        MIDIOUT.sendSysEx(midiIn.getSysExArrayLength(), midiIn.getSysExArray(), true);
        break;

      case midi::SongSelect:
        registerMidiOutActivity(LED_BLINK_DURATION_2_BYTE);
        MIDIOUT.sendSongSelect(midiIn.getData1());
        break;

      case midi::SongPosition:
        registerMidiOutActivity(LED_BLINK_DURATION_2_BYTE);
        twoByteValue = midiIn.getData1() | ((unsigned)midiIn.getData2() << 7);
        MIDIOUT.sendSongPosition(twoByteValue);
        // Keep the song position pointer
        hasSongPosition[portNumber] = true;
        songPosition[portNumber] = twoByteValue;
        break;

      case midi::TimeCodeQuarterFrame:
        registerMidiOutActivity(LED_BLINK_DURATION_3_BYTE);
        MIDIOUT.sendTimeCodeQuarterFrame(midiIn.getData1(), midiIn.getData2());
        break;

      default:
        break;  // LCOV_EXCL_LINE - Unreacheable code, but prevents unhandled case warning.
    }
  }
}

/**
 * Inintialize
 */
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  MIDI0.begin(MIDI_CHANNEL_OMNI);
  MIDI1.begin(MIDI_CHANNEL_OMNI);
  MIDI2.begin(MIDI_CHANNEL_OMNI);
  MIDI3.begin(MIDI_CHANNEL_OMNI);
  MIDI4.begin(MIDI_CHANNEL_OMNI);
  MIDI5.begin(MIDI_CHANNEL_OMNI);
  MIDI6.begin(MIDI_CHANNEL_OMNI);
  MIDI7.begin(MIDI_CHANNEL_OMNI);
  MIDIOUT.begin(MIDI_CHANNEL_OMNI);
  MIDI0.turnThruOff();
  MIDI1.turnThruOff();
  MIDI2.turnThruOff();
  MIDI3.turnThruOff();
  MIDI4.turnThruOff();
  MIDI5.turnThruOff();
  MIDI6.turnThruOff();
  MIDI7.turnThruOff();
  MIDIOUT.turnThruOff();
}

/**
 * Main loop
 */
void loop() {
  mergeMIDI(MIDI0, 0);
  mergeMIDI(MIDI1, 1);
  mergeMIDI(MIDI2, 2);
  mergeMIDI(MIDI3, 3);
  mergeMIDI(MIDI4, 4);
  mergeMIDI(MIDI5, 5);
  mergeMIDI(MIDI6, 6);
  mergeMIDI(MIDI7, 7);
  handleActiveSensingTimeout();
  handleLedOff();
}