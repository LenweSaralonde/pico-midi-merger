/*
 * 8-port MIDI Merger for Raspberry Pi Pico
 */

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

// MIDI baud rate
#define MIDI_BAUD_RATE 31250

// MIDI receive timeout in microseconds
#define MIDI_RECEIVE_TIMEOUT 640

// Duration in microseconds the activity LED remains on after sending a data byte
#define ACTIVITY_LED_DURATION 320

// Active sensing timeout in ms
#define ACTIVE_SENSING_TIMEOUT 5000

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

// FIFO buffers for each port
int buffer[NUM_INPUT_PORTS][FIFO_SIZE] = {};
int bufferStartIndex[NUM_INPUT_PORTS] = {};
int bufferSize[NUM_INPUT_PORTS] = {};

// Running statuses
uint8_t runningStatus[NUM_INPUT_PORTS] = {};

// Last sent MIDI message type
uint8_t lastSentStatusByte = 0x00;

// Activity LED is on or off
bool isActivityLedOn = false;

// Time in microseconds the last data byte was sent to MIDI out
unsigned long lastSentDataByteMicros;

// Master clock port number (0-7 or PORT_NONE)
uint8_t masterClockPort = PORT_NONE;

// Master active sensing port number (0-7 or PORT_NONE)
uint8_t activeSensingPort = PORT_NONE;

// Last time in ms the last active sensing message was received from the active sensing port
unsigned long lastReceivedActiveSensingMillis;

// Keep track of the song position bytes for each port
uint8_t songPosition[NUM_INPUT_PORTS][2] = {};

/**
 * Get the serial port instance for the port number
 * @param portNumber
 * @return SerialPIO instance
 */
SerialPIO& getPort(uint8_t portNumber) {
  switch (portNumber) {
    case 0: return SerialPIO0; break;
    case 1: return SerialPIO1; break;
    case 2: return SerialPIO2; break;
    case 3: return SerialPIO3; break;
    case 4: return SerialPIO4; break;
    case 5: return SerialPIO5; break;
    case 6: return SerialPIO6; break;
    case 7: return SerialPIO7; break;
    default: return SerialPIO0; break;
  }
}

/**
 * Read the MIDI data from the serial port and store it into the port buffer
 * @param portNumber
 */
void readMIDI(uint8_t portNumber) {
  int& startIndex = bufferStartIndex[portNumber];
  int& size = bufferSize[portNumber];

  // Adjust the start index and size to the first valid byte
  // in case some deleted bytes are encountered as the beginning of the buffer.
  while (size > 0 && buffer[portNumber][startIndex] == -1) {
    startIndex = (startIndex + 1) % FIFO_SIZE;
    size--;
  }

  // Read the incoming data from the serial port and store it into the FIFO buffer.
  while (getPort(portNumber).available() > 0) {
    int byte = getPort(portNumber).read();
    if (size < FIFO_SIZE - 1) {
      buffer[portNumber][(startIndex + size) % FIFO_SIZE] = byte;
      size++;
    }
  }
}

/**
 * Return the available bytes for the port number
 * @param portNumber
 * @return number of bytes in the buffer
 */
uint8_t available(uint8_t portNumber) {
  readMIDI(portNumber);
  return bufferSize[portNumber];
}

/**
 * Return the next data byte for the port number
 * @param portNumber
 * @return the next byte or -1 if none
 */
int next(uint8_t portNumber) {
  // The buffer is empty
  if (available(portNumber) == 0) {
    return -1;
  }
  return buffer[portNumber][bufferStartIndex[portNumber]];
}

/**
 * Read one data byte from the port number
 * @param portNumber
 * @return the read byte or -1 if none
 */
int read(uint8_t portNumber) {
  // The buffer is empty
  if (available(portNumber) == 0) {
    return -1;
  }

  // Get current start index and size
  int& startIndex = bufferStartIndex[portNumber];
  int& size = bufferSize[portNumber];

  // Read the byte
  int byte = buffer[portNumber][startIndex];

  // Update start index and size
  startIndex = (startIndex + 1) % FIFO_SIZE;
  size--;

  return byte;
}

/**
 * Time out the master active sensing port if no AS message has been received during the timeout period
 */
void handleActiveSensingTimeout() {
  if (activeSensingPort != PORT_NONE && millis() - lastReceivedActiveSensingMillis > ACTIVE_SENSING_TIMEOUT) {
    activeSensingPort = PORT_NONE;
  }
}

/**
 * Indicates if the data byte is a status byte
 * @param dataByte
 * @return is status byte
 */
bool isStatusByte(int dataByte) {
  return dataByte >= 0x80;
}

/**
 * Indicates if the status byte is for a channel message
 * @param statusByte
 * @return is channel message
 */
bool isChannelMessageType(int statusByte) {
  return statusByte >= 0x80 && statusByte <= 0xEF;
}

/**
 * Indicates if the status byte is for a real-time message
 * @param statusByte
 * @return is real-time
 */
bool isRealTimeMessageType(int statusByte) {
  return statusByte >= 0xF8;
}

/**
 * Indicates if the status byte is for a system exclusive message
 * @param statusByte
 * @return is system exclusive
 */
bool isSystemExclusive(int statusByte) {
  return statusByte == 0xF0 || statusByte == 0xF7;  // System Exclusive Start or System Exclusive End
}

/**
 * Get the MIDI message length in bytes, including the status byte.
 * @param statusByte
 * @return message length or 0 if it can't be determined (SysEx)
 */
uint8_t getMessageLength(uint8_t statusByte) {
  // 1 byte messages
  if (
    statusByte == 0xF4 ||  // (Undefined)
    statusByte == 0xF5 ||  // (Undefined)
    statusByte == 0xF6 ||  // TuneRequest
    statusByte == 0xF8 ||  // Clock
    statusByte == 0xF9 ||  // Tick
    statusByte == 0xFA ||  // Start
    statusByte == 0xFB ||  // Continue
    statusByte == 0xFC ||  // Stop
    statusByte == 0xFD ||  // (Undefined)
    statusByte == 0xFE ||  // ActiveSensing
    statusByte == 0xFF     // SystemReset)
  ) {
    return 1;
  }

  // 2 bytes messages
  if (
    statusByte >= 0xC0 && statusByte <= 0xCF ||  // ProgramChange
    statusByte >= 0xD0 && statusByte <= 0xDF ||  // AfterTouchChannel
    statusByte == 0xF1 ||                        // TimeCodeQuarterFrame
    statusByte == 0xF3                           // SongSelect
  ) {
    return 2;
  }

  // 3 bytes messages
  if (
    statusByte >= 0x90 && statusByte <= 0x9F ||  // NoteOn
    statusByte >= 0x80 && statusByte <= 0x8F ||  // NoteOff
    statusByte >= 0xA0 && statusByte <= 0xAF ||  // AfterTouchPoly
    statusByte >= 0xB0 && statusByte <= 0xBF ||  // ControlChange
    statusByte >= 0xE0 && statusByte <= 0xEF ||  // PitchBend
    statusByte == 0xF2                           // SongPosition
  ) {
    return 3;
  }

  // Undefined length
  return 0;
}

/**
 * Turn the activity LED off if the delay has expired.
 */
void handleLedOff() {
  if (isActivityLedOn && micros() - lastSentDataByteMicros > ACTIVITY_LED_DURATION) {
    isActivityLedOn = false;
    digitalWrite(LED_BUILTIN, LOW);
  }
}

/**
 * Send a data byte to MIDI out
 * @param dataByte
 */
void sendDataByte(uint8_t dataByte) {
  // Turn the activity LED on
  digitalWrite(LED_BUILTIN, HIGH);
  isActivityLedOn = true;
  // Send byte
  Serial1.write(dataByte);
  lastSentDataByteMicros = micros();
}

/**
 * Send status byte from provided port number to MIDI out, if applicable
 * @param statusByte
 * @param portNumber
 */
void sendStatusByte(uint8_t statusByte, uint8_t portNumber) {
  // Attempting to send a master clock message while not being the master clock
  if (statusByte == 0xF8 && portNumber != masterClockPort) {
    return;
  }

  // Attempting to send an active sensing while not being the master AS port
  if (statusByte == 0xFE && portNumber != activeSensingPort) {
    return;
  }

  // Do not send the status byte if there is an outgoing running status
  if (isChannelMessageType(statusByte) && statusByte == lastSentStatusByte) {
    return;
  }

  // Send status byte to MIDI out
  sendDataByte(statusByte);
  lastSentStatusByte = statusByte;
}

/**
 * Process real-time message to determine the master clock and/or master active sensing port
 * @param statusByte
 * @param portNumber
 */
void processRealTimeMessage(uint8_t statusByte, uint8_t portNumber) {
  // Song start or song continue with position set to 0 received: set port number as master clock
  if (statusByte == 0xFA || statusByte == 0xFB && songPosition[portNumber][0] == 0x00 && songPosition[portNumber][1] == 0x00) {
    masterClockPort = portNumber;
  }
  // Active sensing
  else if (statusByte == 0xFE) {
    // Set AS port if undefined
    if (activeSensingPort == PORT_NONE) {
      activeSensingPort = portNumber;
      lastReceivedActiveSensingMillis = millis();
    }
    // Otherwise, update the last encountered AS message timestamp
    else if (activeSensingPort == portNumber) {
      lastReceivedActiveSensingMillis = millis();
    }
  }
}

/**
 * Forward real-time messages pending from all the input ports,
 * regardless of their position in the queue
 */
void mergeRealTimeMessages() {
  for (uint8_t portNumber = 0; portNumber < NUM_INPUT_PORTS; portNumber++) {
    if (available(portNumber) > 0) {
      for (int index = 0; index < bufferSize[portNumber]; index++) {
        int bufferIndex = (index + bufferStartIndex[portNumber]) % FIFO_SIZE;
        int dataByte = buffer[portNumber][bufferIndex];
        if (isRealTimeMessageType(dataByte)) {
          sendStatusByte(dataByte, portNumber);
          buffer[portNumber][bufferIndex] = -1;  // "delete" the read byte
        }
      }
    }
  }
}

/**
 * Merge incoming MIDI message for provided port number
 * @param portNumber
 */
void mergeMIDI(uint8_t portNumber) {
  // Nothing to read here
  if (available(portNumber) == 0)
    return;

  // Determine the type of message based on the first data byte
  uint8_t nextByte = next(portNumber);

  // Get and send status byte
  uint8_t statusByte;

  // It's a proper status byte
  if (isStatusByte(nextByte)) {
    statusByte = read(portNumber);
    sendStatusByte(statusByte, portNumber);
    processRealTimeMessage(statusByte, portNumber);
  }
  // Check if we have a valid running status for this port,
  // in this case we're reading the first data byte
  else if (runningStatus[portNumber] != 0x00) {
    statusByte = runningStatus[portNumber];
    sendStatusByte(statusByte, portNumber);
  }
  // This is an error
  else {
    // Read all the junk bytes until a proper status byte is encountered to try again in the next loop
    while (available(portNumber) > 0 && !isStatusByte(next(portNumber))) {
      read(portNumber);
    }
    return;
  }

  // Determine the number of bytes left or if it's a SysEx
  uint8_t messageLength = getMessageLength(statusByte);
  bool isSysEx = isSystemExclusive(statusByte);
  uint8_t bytesLeft = (!isSysEx && messageLength > 1) ? messageLength - 1 : 0;

  // Store running status for channel messages
  if (isChannelMessageType(statusByte)) {
    runningStatus[portNumber] = statusByte;
  }
  // Reset running status
  else if (!isRealTimeMessageType(statusByte)) {
    runningStatus[portNumber] = 0x00;
  }

  // Forward the message data to MIDI out
  unsigned int lastReceivedTime = micros();
  while (bytesLeft > 0 || isSysEx) {
    // Handle timeout
    if (micros() - lastReceivedTime > MIDI_RECEIVE_TIMEOUT) {
      handleLedOff();
      return;
    }

    // Interleave real-time messages
    mergeRealTimeMessages();

    // There is some data to read
    uint8_t dataByte;
    if (available(portNumber) > 0) {

      // Terminate the current SysEx message if the next byte is a status byte
      nextByte = next(portNumber);
      if (isSysEx && isStatusByte(nextByte) && !isRealTimeMessageType(nextByte) && !isSystemExclusive(nextByte)) {
        handleLedOff();
        return;
      }

      // Read and forward data byte
      dataByte = read(portNumber);
      sendDataByte(dataByte);
      lastReceivedTime = micros();

      // If was not a real-time message
      if (!isRealTimeMessageType(dataByte)) {

        // Keep track of the song position to determine the master clock port
        if (statusByte == 0xF2) {
          if (bytesLeft == 2) {
            songPosition[portNumber][0] = dataByte;
          } else if (bytesLeft == 1) {
            songPosition[portNumber][1] = dataByte;
          }
        }

        // One byte less to read
        if (!isSysEx) {
          bytesLeft--;
        }
        // End of SysEx message
        else if (dataByte == 0xF7) {
          isSysEx = false;
        }
      } else {
        processRealTimeMessage(dataByte, portNumber);
      }
    } else {
      handleLedOff();
    }
  }
}

/**
 * Reset merger
 */
void reset() {
  digitalWrite(LED_BUILTIN, LOW);
  isActivityLedOn = false;
  lastSentStatusByte = 0x00;
  masterClockPort = PORT_NONE;
  activeSensingPort = PORT_NONE;
  for (uint8_t portNumber = 0; portNumber < NUM_INPUT_PORTS; portNumber++) {
    bufferStartIndex[portNumber] = 0;
    bufferSize[portNumber] = 0;
    runningStatus[portNumber] = 0x00;
    songPosition[portNumber][0] = 0xFF;
    songPosition[portNumber][1] = 0xFF;
  }
}

/**
 * Initialize
 */
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial1.begin(MIDI_BAUD_RATE);
  for (uint8_t portNumber = 0; portNumber < NUM_INPUT_PORTS; portNumber++) {
    getPort(portNumber).begin(MIDI_BAUD_RATE);
  }
  reset();
}

/**
 * Main loop
 */
void loop() {
  for (uint8_t portNumber = 0; portNumber < NUM_INPUT_PORTS; portNumber++) {
    mergeMIDI(portNumber);
  }
  handleActiveSensingTimeout();
  handleLedOff();
}