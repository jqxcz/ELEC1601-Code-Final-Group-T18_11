#include <SoftwareSerial.h>   //Software Serial Port
#include <Servo.h>

#define SERVO_LEFT 10
#define SERVO_RIGHT 11

#define IR_TX_LEFT 12
#define IR_RX_LEFT 13
#define LED_LEFT 9

#define PING_FRONT 5


#define IR_TX_RIGHT 2
#define IR_RX_RIGHT 3
#define LED_RIGHT 4

#define RxD 7
#define TxD 6
#define ConnStatus A1
#define DEBUG_ENABLED  1

int shieldPairNumber = 11;

// CAUTION: If ConnStatusSupported = true you MUST NOT use pin A1 otherwise "random" reboots will occur
// CAUTION: If ConnStatusSupported = true you MUST set the PIO[1] switch to A1 (not NC)

boolean ConnStatusSupported = true;   // Set to "true" when digital connection status is available on Arduino pin

// #######################################################

// The following two string variable are used to simplify adaptation of code to different shield pairs

String slaveNameCmd = "\r\n+STNA=Slave";   // This is concatenated with shieldPairNumber later

Servo servoLeft;
Servo servoRight;
SoftwareSerial blueToothSerial(RxD,TxD);

void setup() {
  Serial.begin(9600);

  //SET SERVO
  servoLeft.attach(SERVO_LEFT); servoRight.attach(SERVO_RIGHT);
  servoLeft.writeMicroseconds(1500);
  servoRight.writeMicroseconds(1500);

  //SET PINS
  pinMode(LED_LEFT, OUTPUT); pinMode(LED_RIGHT, OUTPUT);
  pinMode(IR_TX_LEFT, OUTPUT); pinMode(IR_RX_LEFT,INPUT);
  pinMode(IR_TX_RIGHT, OUTPUT); pinMode(IR_RX_RIGHT,INPUT);

  //INITIALISE BLUETOOTH
  blueToothSerial.begin(38400); // Set Bluetooth module to default baud rate 38400

  pinMode(RxD, INPUT); pinMode(TxD, OUTPUT); pinMode(ConnStatus, INPUT);

    //  Check whether Master and Slave are already connected by polling the ConnStatus pin (A1 on SeeedStudio v1 shield)
    //  This prevents running the full connection setup routine if not necessary.

    if(ConnStatusSupported) Serial.println("Checking Slave-Master connection status.");

    if(ConnStatusSupported && digitalRead(ConnStatus)==1)
    {
        Serial.println("Already connected to Master - remove USB cable if reboot of Master Bluetooth required.");
    }
    else
    {
        Serial.println("Not connected to Master.");

        setupBlueToothConnection();   // Set up the local (slave) Bluetooth module

        delay(1000);                  // Wait one second and flush the serial buffers
        Serial.flush();
        blueToothSerial.flush();
    }

}

#define DETECTION 0
#define NO_DETECTION 1

char moves[300];
int moves_index = 0;

int left_detect = NO_DETECTION;
int front_detect = NO_DETECTION;
int right_detect = NO_DETECTION;

int right_fine_detect = NO_DETECTION;
int left_fine_detect = NO_DETECTION;

#define HALT 1500
#define CLOCKWISE 1300
#define ANTI_CLOCKWISE 1700

#define FORWARDS 1
#define LEFT 2
#define RIGHT 3
#define BACKWARDS 4

int right_counter = 0;
int left_counter = 0;

#define OFF 0
#define MANUAL 1
#define AUTOMATIC 2

//IR CALIBRATION
#define FRONT_DISTANCE 9
#define RIGHT_DETECTION_BEFORE_ADJUST 5
#define LEFT_DETECTION_BEFORE_ADJUST 5
#define LEFT_FREQ 36100 //calibrated for Parallax IR Tx and Rx @ 10kOhms
#define RIGHT_FREQ 36100 //calibrated for Parallax IR Tx and Rx @ 10kOhms
#define TURNING_FREQ 38000 //Set at best detection frequency

#define BUF_SIZE 20
int mode = MANUAL; //change to back to manual
int x, l, r;
long distance = 0;//checkDistance();
char input_buffer[BUF_SIZE]; //chars are 1byte in size

void loop() {
  while (true) {
    //Serial.print("Mode: ");
    //Serial.println(mode);
    if (blueToothSerial.available()) {
      x = blueToothSerial.readBytesUntil('\n', input_buffer, BUF_SIZE);
      input_buffer[x-1]='\0';

      Serial.print("Buffer contents: ");
      Serial.println(input_buffer);

    } else {
      input_buffer[0] = '\0';
    }

    if (input_buffer[0] == 'a') {
      mode = AUTOMATIC;
    }  else if (input_buffer[0] == 'm') {
      mode = MANUAL;
      moves_index = 0;
    } else if (mode == MANUAL) {
      //digitalWrite(LED_RIGHT,LOW); //TEMP FOR DEBUGGING
      //digitalWrite(LED_LEFT,HIGH); //TEMP FOR DEBUGGING
      if (sscanf(input_buffer, "%d %d", &l, &r) != 2) {
        continue;
      }
      servoLeft.writeMicroseconds(l);
      servoRight.writeMicroseconds(r);
    } else if (mode == AUTOMATIC) {

      //digitalWrite(LED_LEFT,LOW); //TEMP FOR DEBUGGING
      //digitalWrite(LED_RIGHT,HIGH); //TEMP FOR DEBUGGING

      run_detect();

      //FOR DEBUGGING ONLY
      Serial.print("Distance: ");
      Serial.print(distance);
      Serial.print("cm | L: ");
      Serial.print(left_detect);
      Serial.print(" F: ");
      Serial.print(front_detect);
      Serial.print(" R: ");
      Serial.print(right_detect);
      Serial.print(" MOVE INDEX: ");
      Serial.print(moves_index);
      Serial.print(" | LCOUNT: ");
      Serial.print(left_counter);
      Serial.print(" RCOUNT: ");
      Serial.println(right_counter);

      if (front_detect == DETECTION && right_detect == NO_DETECTION) {
        right_turn();
        moves[moves_index++] = RIGHT;
      } else if (front_detect == DETECTION && left_detect == NO_DETECTION) {
        left_turn();
        moves[moves_index++] = LEFT;
      } else if (front_detect == DETECTION && left_detect == DETECTION && right_detect == DETECTION) {
        pickup_and_reverse();
      } else if (front_detect == NO_DETECTION) {
        run_align();
        if (right_counter == RIGHT_DETECTION_BEFORE_ADJUST) {
          left_align();
          right_counter = 0;
        } else if (left_counter == LEFT_DETECTION_BEFORE_ADJUST) {
          right_align();
          left_counter = 0;
        }
        forwards();
        moves[moves_index++] = FORWARDS;
      }
    }
  }
}

void stops() {
  Serial.println("STOPPING");
  servoLeft.writeMicroseconds(1500);
  servoRight.writeMicroseconds(1500);
  delay(1000);
}

void forwards() {
  Serial.println("GOING FORWARDS");
  servoLeft.writeMicroseconds(1500+200);
  servoRight.writeMicroseconds(1500-200);
  delay(100);
}

void back() {
  Serial.println("GOING BACKWARDS");
  servoLeft.writeMicroseconds(1500-200);
  servoRight.writeMicroseconds(1500+200);
  delay(200);
}

void right_turn() {
  Serial.println("TURNING RIGHT");
  servoLeft.writeMicroseconds(1500+50);
  servoRight.writeMicroseconds(1500+50);
  delay(675); //This is calibrated to 90 degree turn
}

void right_align() {
  Serial.println("ALIGNING RIGHT");
  servoLeft.writeMicroseconds(1500+20);
  servoRight.writeMicroseconds(1500+20);
  delay(200);
}

void left_turn() {
  Serial.println("TURNING LEFT");
  servoLeft.writeMicroseconds(1500-50);
  servoRight.writeMicroseconds(1500-50);
  delay(720); //This is calibrated to 90 degree turn
}

void left_align() {
  Serial.println("ALIGNING LEFT");
  servoLeft.writeMicroseconds(1500-20);
  servoRight.writeMicroseconds(1500-20);
  delay(200);

}

int run_align() {
  left_fine_detect = irDetect(IR_TX_LEFT, IR_RX_LEFT, LEFT_FREQ);
  digitalWrite(LED_LEFT,!left_fine_detect);
  if (left_fine_detect == DETECTION) {
    left_counter++;
  } else {
    left_counter = 0;
  }

  right_fine_detect = irDetect(IR_TX_RIGHT, IR_RX_RIGHT, RIGHT_FREQ);
  digitalWrite(LED_RIGHT,!right_fine_detect);
  if (right_fine_detect == DETECTION) {
    right_counter++;
  } else {
    right_counter = 0;
  }
}

void run_detect() {
  distance = checkDistance();


  left_detect = irDetect(IR_TX_LEFT, IR_RX_LEFT, TURNING_FREQ);
  //digitalWrite(LED_LEFT,!left_detect);

  if (distance < FRONT_DISTANCE) {
    front_detect = DETECTION;
  } else {
    front_detect = NO_DETECTION;
  }
  //digitalWrite(LED_FRONT,!front_detect);

  right_detect = irDetect(IR_TX_RIGHT, IR_RX_RIGHT, TURNING_FREQ);
  //digitalWrite(LED_RIGHT,!right_detect);

}

void pickup_and_reverse() {

  servoLeft.writeMicroseconds(HALT);
  servoRight.writeMicroseconds(HALT);

  blueToothSerial.println("f");

  for (int i = 0; i <30; i++) {
    digitalWrite(LED_LEFT, HIGH);
    digitalWrite(LED_RIGHT, HIGH);
    delay(100);
    digitalWrite(LED_LEFT, LOW);
    digitalWrite(LED_RIGHT, LOW);
    delay(100);

  }
  //Moves forward to attatch object
  //forwards();
  //back();

  //NAVIGATE BACK THROUGH AUTO MOVEMENTS
  while (moves_index >= 0) {

    run_align();
    if (right_counter == RIGHT_DETECTION_BEFORE_ADJUST) {
      right_align();
      right_counter = 0;
    } else if (left_counter == LEFT_DETECTION_BEFORE_ADJUST) {
      left_align();
      left_counter = 0;
    }

    Serial.println(moves_index);
    if (moves[moves_index] == LEFT) {
      right_turn();
    }
    else if (moves[moves_index] == RIGHT) {
      left_turn();
    }
    else if (moves[moves_index] == FORWARDS) {
      back();
    }
    else if (moves[moves_index] == BACKWARDS) {
      forwards();
    }
    moves_index--;
  }
  stops();
  stops();
  //NAVIGATE BACK THROUGH AUTO MOVEMENTS
  blueToothSerial.println("r");
  delay(400);
  Serial.println("MANUAL REVERSE");

  while (mode == AUTOMATIC) {
    if (blueToothSerial.available()) {
      x = blueToothSerial.readBytesUntil('\n', input_buffer, BUF_SIZE);
      input_buffer[x-1]='\0';
      Serial.print("Buffer contents: ");
      Serial.println(input_buffer);
    } else {
      input_buffer[0] = '\0';
    }

    if (input_buffer[0] == 'm') {
      mode = MANUAL;
      break;
    }

    if (sscanf(input_buffer, "%d %d", &l, &r) != 2) {
      continue;
    }
    servoLeft.writeMicroseconds(l);
    servoRight.writeMicroseconds(r);
  }
  servoLeft.writeMicroseconds(HALT);
  servoRight.writeMicroseconds(HALT);
  Serial.println("MANUAL REVERSE COMPLETE");
}

long checkDistance() {
  delay(100);
   pinMode(PING_FRONT, OUTPUT);
   digitalWrite(PING_FRONT, LOW);
   delayMicroseconds(5);
   digitalWrite(PING_FRONT, HIGH);
   delayMicroseconds(5);
   digitalWrite(PING_FRONT, LOW);

  pinMode(PING_FRONT, INPUT);
  long duration = pulseIn(PING_FRONT, HIGH);
  pinMode(PING_FRONT, OUTPUT);
  delay(5);
  return duration /58; // Calculation based off speed of sound (29ms per cm divided by there and back)
 }

int irDetect(int irLedPin, int irReceiverPin, long frequency) {
  tone(irLedPin, frequency, 8); // IRLED 38 kHz at least 1 ms
  delay(1); // Wait 1 ms (allow light)
  int ir = digitalRead(irReceiverPin); // IR receiver -> ir variable
  delay(1); // Down time before recheck

  return ir; // Return 1 no detect, 0 detect
}

void setupBlueToothConnection()
{
    Serial.println("Setting up the local (slave) Bluetooth module.");

    slaveNameCmd += shieldPairNumber;
    slaveNameCmd += "\r\n";

    blueToothSerial.print("\r\n+STWMOD=0\r\n");      // Set the Bluetooth to work in slave mode
    blueToothSerial.print(slaveNameCmd);             // Set the Bluetooth name using slaveNameCmd
    blueToothSerial.print("\r\n+STAUTO=0\r\n");      // Auto-connection should be forbidden here
    blueToothSerial.print("\r\n+STOAUT=1\r\n");      // Permit paired device to connect me

    //  print() sets up a transmit/outgoing buffer for the string which is then transmitted via interrupts one character at a time.
    //  This allows the program to keep running, with the transmitting happening in the background.
    //  Serial.flush() does not empty this buffer, instead it pauses the program until all Serial.print()ing is done.
    //  This is useful if there is critical timing mixed in with Serial.print()s.
    //  To clear an "incoming" serial buffer, use while(Serial.available()){Serial.read();}

    blueToothSerial.flush();
    delay(2000);                                     // This delay is required

    blueToothSerial.print("\r\n+INQ=1\r\n");         // Make the slave Bluetooth inquirable

    blueToothSerial.flush();
    delay(2000);                                     // This delay is required

    Serial.println("The slave bluetooth is inquirable!");
}
