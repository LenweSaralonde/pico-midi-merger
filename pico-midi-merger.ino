/*
 * 8-port MIDI Merger for Raspberry Pi Pico
 */

#include <MIDI.h>

// Using UART0 TX for MIDI Out

// GPIO MIDI In pins
#define GP_IN_1 2  // Physical pin 4
#define GP_IN_2 3  // Physical pin 5
#define GP_IN_3 4  // Physical pin 6
#define GP_IN_4 5  // Physical pin 7
#define GP_IN_5 6  // Physical pin 9
#define GP_IN_6 7  // Physical pin 10
#define GP_IN_7 8  // Physical pin 11
#define GP_IN_8 9  // Physical pin 12

// FIFO size
#define FIFO_SIZE 256

// Create input serial ports
SerialPIO SerialPIO1(SerialPIO::NOPIN, GP_IN_1, FIFO_SIZE);
SerialPIO SerialPIO2(SerialPIO::NOPIN, GP_IN_2, FIFO_SIZE);
SerialPIO SerialPIO3(SerialPIO::NOPIN, GP_IN_3, FIFO_SIZE);
SerialPIO SerialPIO4(SerialPIO::NOPIN, GP_IN_4, FIFO_SIZE);
SerialPIO SerialPIO5(SerialPIO::NOPIN, GP_IN_5, FIFO_SIZE);
SerialPIO SerialPIO6(SerialPIO::NOPIN, GP_IN_6, FIFO_SIZE);
SerialPIO SerialPIO7(SerialPIO::NOPIN, GP_IN_7, FIFO_SIZE);
SerialPIO SerialPIO8(SerialPIO::NOPIN, GP_IN_8, FIFO_SIZE);

// Create MIDI instances
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO1, MIDI1);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO2, MIDI2);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO3, MIDI3);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO4, MIDI4);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO5, MIDI5);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO6, MIDI6);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO7, MIDI7);
MIDI_CREATE_INSTANCE(SerialPIO, SerialPIO8, MIDI8);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDIOUT);

/**
 * Forwards the MIDI message to MIDIOUT
 * @param type
 * @param channel
 * @param data1
 * @param data2
 * @param sysExArray
 */
void forwardMIDI(midi::MidiType type, byte channel, byte data1, byte data2, const byte* sysExArray) {
  if (type != midi::SystemExclusive) {
    MIDIOUT.send(type, data1, data2, channel);
  } else {
    int sysExLen = data1 + 256 * data2;
    MIDIOUT.sendSysEx(sysExLen, sysExArray, true);
  }
}

/**
 * Inintialize
 */
void setup() {
  MIDI1.begin(MIDI_CHANNEL_OMNI);
  MIDI2.begin(MIDI_CHANNEL_OMNI);
  MIDI3.begin(MIDI_CHANNEL_OMNI);
  MIDI4.begin(MIDI_CHANNEL_OMNI);
  MIDI5.begin(MIDI_CHANNEL_OMNI);
  MIDI6.begin(MIDI_CHANNEL_OMNI);
  MIDI7.begin(MIDI_CHANNEL_OMNI);
  MIDI8.begin(MIDI_CHANNEL_OMNI);
  MIDIOUT.begin(MIDI_CHANNEL_OMNI);
  MIDI1.turnThruOff();
  MIDI2.turnThruOff();
  MIDI3.turnThruOff();
  MIDI4.turnThruOff();
  MIDI5.turnThruOff();
  MIDI6.turnThruOff();
  MIDI7.turnThruOff();
  MIDI8.turnThruOff();
  MIDIOUT.turnThruOff();
}

/**
 * Main loop
 */
void loop() {
  if (MIDI1.read()) {
    forwardMIDI(MIDI1.getType(), MIDI1.getChannel(), MIDI1.getData1(), MIDI1.getData2(), MIDI1.getSysExArray());
  }
  if (MIDI2.read()) {
    forwardMIDI(MIDI2.getType(), MIDI2.getChannel(), MIDI2.getData1(), MIDI2.getData2(), MIDI2.getSysExArray());
  }
  if (MIDI3.read()) {
    forwardMIDI(MIDI3.getType(), MIDI3.getChannel(), MIDI3.getData1(), MIDI3.getData2(), MIDI3.getSysExArray());
  }
  if (MIDI4.read()) {
    forwardMIDI(MIDI4.getType(), MIDI4.getChannel(), MIDI4.getData1(), MIDI4.getData2(), MIDI4.getSysExArray());
  }
  if (MIDI5.read()) {
    forwardMIDI(MIDI5.getType(), MIDI5.getChannel(), MIDI5.getData1(), MIDI5.getData2(), MIDI5.getSysExArray());
  }
  if (MIDI6.read()) {
    forwardMIDI(MIDI6.getType(), MIDI6.getChannel(), MIDI6.getData1(), MIDI6.getData2(), MIDI6.getSysExArray());
  }
  if (MIDI7.read()) {
    forwardMIDI(MIDI7.getType(), MIDI7.getChannel(), MIDI7.getData1(), MIDI7.getData2(), MIDI7.getSysExArray());
  }
  if (MIDI8.read()) {
    forwardMIDI(MIDI8.getType(), MIDI8.getChannel(), MIDI8.getData1(), MIDI8.getData2(), MIDI8.getSysExArray());
  }
}