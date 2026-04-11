// Hoverboard ovládání páčkou plynu - HW Serial
// Pravý sideboard (USART3), 9600 baud
// DŮLEŽITÉ: Při nahrávání kódu ODPOJ dráty z D0/D1!

#define HOVER_SERIAL_BAUD   9600
#define START_FRAME         0xABCD
#define TIME_SEND           50

#define THROTTLE_PIN        A0
#define THROTTLE_MIN        178
#define THROTTLE_MAX        850
#define SPEED_MAX           500

#include <SoftwareSerial.h>
SoftwareSerial DebugSerial(4, 5);  // Debug na D4/D5 (volitelné, nemusíš zapojovat)

// Struktury
typedef struct {
  uint16_t start;
  int16_t  steer;
  int16_t  speed;
  uint16_t checksum;
} SerialCommand;
SerialCommand Command;

typedef struct {
  uint16_t start;
  int16_t  cmd1;
  int16_t  cmd2;
  int16_t  speedR_meas;
  int16_t  speedL_meas;
  int16_t  batVoltage;
  int16_t  boardTemp;
  uint16_t cmdLed;
  uint16_t checksum;
} SerialFeedback;
SerialFeedback Feedback;
SerialFeedback NewFeedback;

uint8_t idx = 0;
uint16_t bufStartFrame;
byte *p;
byte incomingByte;
byte incomingBytePrev;

void setup() {
  Serial.begin(HOVER_SERIAL_BAUD);      // HW Serial = hoverboard
  DebugSerial.begin(115200);             // SW Serial = debug (volitelné)
  DebugSerial.println("Hoverboard Throttle HW Serial v1.0");
  pinMode(LED_BUILTIN, OUTPUT);
}

void Send(int16_t uSteer, int16_t uSpeed) {
  Command.start    = (uint16_t)START_FRAME;
  Command.steer    = (int16_t)uSteer;
  Command.speed    = (int16_t)uSpeed;
  Command.checksum = (uint16_t)(Command.start ^ Command.steer ^ Command.speed);
  Serial.write((uint8_t *) &Command, sizeof(Command));
}

void Receive() {
  if (Serial.available()) {
    incomingByte    = Serial.read();
    bufStartFrame   = ((uint16_t)(incomingByte) << 8) | incomingBytePrev;
  } else {
    return;
  }

  if (bufStartFrame == START_FRAME) {
    p       = (byte *)&NewFeedback;
    *p++    = incomingBytePrev;
    *p++    = incomingByte;
    idx     = 2;
  } else if (idx >= 2 && idx < sizeof(SerialFeedback)) {
    *p++    = incomingByte;
    idx++;
  }

  if (idx == sizeof(SerialFeedback)) {
    uint16_t checksum;
    checksum = (uint16_t)(NewFeedback.start ^ NewFeedback.cmd1 ^ NewFeedback.cmd2 ^ NewFeedback.speedR_meas ^ NewFeedback.speedL_meas
                        ^ NewFeedback.batVoltage ^ NewFeedback.boardTemp ^ NewFeedback.cmdLed);

    if (NewFeedback.start == START_FRAME && checksum == NewFeedback.checksum) {
      memcpy(&Feedback, &NewFeedback, sizeof(SerialFeedback));
    }
    idx = 0;
  }

  incomingBytePrev = incomingByte;
}

unsigned long iTimeSend = 0;

void loop() {
  unsigned long timeNow = millis();

  Receive();

  if (iTimeSend > timeNow) return;
  iTimeSend = timeNow + TIME_SEND;

  int throttleRaw = analogRead(THROTTLE_PIN);

  int speed = 0;
  if (throttleRaw > THROTTLE_MIN) {
    speed = map(throttleRaw, THROTTLE_MIN, THROTTLE_MAX, 0, SPEED_MAX);
    speed = constrain(speed, 0, SPEED_MAX);
  }

  Send(0, speed);

  digitalWrite(LED_BUILTIN, (timeNow % 2000) < 1000);
}
