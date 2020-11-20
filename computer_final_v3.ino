//-----------------------------------------------------------------------------------------------------------//
//                                                                                                           //
//  Slave_ELEC1601_Student_2019_v3                                                                           //
//  The Instructor version of this code is identical to this version EXCEPT it also sets PIN codes           //
//  20191008 Peter Jones                                                                                     //
//                                                                                                           //
//  Bi-directional passing of serial inputs via Bluetooth                                                    //
//  Note: the void loop() contents differ from "capitalise and return" code                                  //
//                                                                                                           //
//  This version was initially based on the 2011 Steve Chang code but has been substantially revised         //
//  and heavily documented throughout.                                                                       //
//                                                                                                           //
//  20190927 Ross Hutton                                                                                     //
//  Identified that opening the Arduino IDE Serial Monitor asserts a DTR signal which resets the Arduino,    //
//  causing it to re-execute the full connection setup routine. If this reset happens on the Slave system,   //
//  re-running the setup routine appears to drop the connection. The Master is unaware of this loss and      //
//  makes no attempt to re-connect. Code has been added to check if the Bluetooth connection remains         //
//  established and, if so, the setup process is bypassed.                                                   //
//                                                                                                           //
//-----------------------------------------------------------------------------------------------------------//


#include <SoftwareSerial.h>   // Software Serial Port


#define RxD 7
#define TxD 6
#define ConnStatus A1         // Connection status on the SeeedStudio v1 shield is available on pin A1
                              // See also ConnStatusSupported boolean below
#define DEBUG_ENABLED 1

#define JOY_X A5
#define JOY_Y A4
#define MODE_BUTTON 2
#define MODE_LED 3
#define BUZZER 9

/*************************************************

 * Public Constants

 *************************************************/

#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_CS1 35
#define NOTE_D1  37
#define NOTE_DS1 39
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_FS1 46
#define NOTE_G1  49
#define NOTE_GS1 52
#define NOTE_A1  55
#define NOTE_AS1 58
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_CS2 69
#define NOTE_D2  73
#define NOTE_DS2 78
#define NOTE_E2  82
#define NOTE_F2  87
#define NOTE_FS2 93
#define NOTE_G2  98
#define NOTE_GS2 104
#define NOTE_A2  110
#define NOTE_AS2 117
#define NOTE_B2  123
#define NOTE_C3  131
#define NOTE_CS3 139
#define NOTE_D3  147
#define NOTE_DS3 156
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349
#define NOTE_DS7 2489
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_FS7 2960
#define NOTE_G7  3136
#define NOTE_GS7 3322
#define NOTE_A7  3520
#define NOTE_AS7 3729
#define NOTE_B7  3951
#define NOTE_C8  4186
#define NOTE_CS8 4435
#define NOTE_D8  4699
#define NOTE_DS8 4978



// ##################################################################################
// ### EDIT THE LINES BELOW TO MATCH YOUR SHIELD NUMBER AND CONNECTION PIN OPTION ###
// ##################################################################################

int shieldPairNumber = 11;

// CAUTION: If ConnStatusSupported = true you MUST NOT use pin A1 otherwise "random" reboots will occur
// CAUTION: If ConnStatusSupported = true you MUST set the PIO[1] switch to A1 (not NC)

boolean ConnStatusSupported = true;   // Set to "true" when digital connection status is available on Arduino pin

// #######################################################

// The following four string variable are used to simplify adaptation of code to different shield pairs

String slaveName = "Slave";                  // This is concatenated with shieldPairNumber later
String masterNameCmd = "\r\n+STNA=Master";   // This is concatenated with shieldPairNumber later
String connectCmd = "\r\n+CONN=";            // This is concatenated with slaveAddr later

int nameIndex = 0;
int addrIndex = 0;

String recvBuf;
String slaveAddr;
String retSymb = "+RTINQ=";   // Start symble when there's any return

boolean interrupt_fired;

SoftwareSerial blueToothSerial(RxD,TxD);

void setup() {

  Serial.begin(9600);


  //SET PINS
  pinMode(MODE_BUTTON, INPUT);
  pinMode(MODE_LED, OUTPUT); pinMode(BUZZER, OUTPUT);
  pinMode(JOY_X, INPUT); pinMode(JOY_Y, INPUT);

  //BUTTON INTERRUPT
  attachInterrupt(0, interrupt_service_routine, FALLING);
  interrupt_fired = false;

  //SETUP BLUETOOTH
  blueToothSerial.begin(38400); // Set Bluetooth module to default baud rate 38400
  pinMode(RxD, INPUT); pinMode(TxD, OUTPUT);
  pinMode(ConnStatus, INPUT);

  //  Check whether Master and Slave are already connected by polling the ConnStatus pin (A1 on SeeedStudio v1 shield)
  //  This prevents running the full connection setup routine if not necessary.

  if(ConnStatusSupported) Serial.println("Checking Master-Slave connection status.");

  if(ConnStatusSupported && digitalRead(ConnStatus)==1) {
    Serial.println("Already connected to Slave - remove USB cable if reboot of Master Bluetooth required.");
  } else{
    Serial.println("Not connected to Slave.");

    setupBlueToothConnection();     // Set up the local (master) Bluetooth module
    //getSlaveAddress();              // Search for (MAC) address of slave
    //makeBlueToothConnection();      // Execute the connection to the slave

    delay(1000);                    // Wait one second and flush the serial buffers
    Serial.flush();
    blueToothSerial.flush();

  }
  //flush out all initial crap
  while (blueToothSerial.available()) {
    blueToothSerial.read();
  }

}

//MANUAL MEMORY SIZE
#define MEM_SIZE 150 // the Slave does not have enough memory to store moves.

//MODES
#define OFF 0
#define MANUAL 1
#define AUTO 2

int joystick_x;
int joystick_y;

int button_pressed;
int last_press;
int mode = OFF;

int mem_x[MEM_SIZE];
int mem_y[MEM_SIZE];
int mem_counter = 0;

boolean mode_changed = false;

#define BUF_SIZE 20
char input_buffer[BUF_SIZE]; //chars are 1byte in size
int x;

void interrupt_service_routine() {
  interrupt_fired = true;
}


void loop() {

  delay(200);
  if (interrupt_fired) {
    //button_pressed = digitalRead(MODE_BUTTON);
    //if (button_pressed != last_press) {
      if (mode == OFF || mode == AUTO) {
        mode = MANUAL;
        blueToothSerial.println("m");
      } else if (mode == MANUAL) {
        mode = AUTO;
        blueToothSerial.println("a");
      }
      //last_press = button_pressed;
    //}
    interrupt_fired = false;
  }

  //DEBUGGING
  Serial.println("---");
  Serial.print("Mode: ");
  Serial.print(mode);
  Serial.print( " Mem Counter: ");
  Serial.println(mem_counter);

  if (mode == MANUAL) {
    digitalWrite(MODE_LED, HIGH);
    read_joystick();

    //DEBUGGING
    //Serial.print("X: ");
    //Serial.println(joystick_x);
    //Serial.print("Y: ");
    //Serial.println(joystick_y);

    blueToothSerial.print(1500 + joystick_x);
    blueToothSerial.print(' ');
    blueToothSerial.println(1500 - joystick_y);
    delay(200); // DO I NEED THIS?

    //Store manual memory
    if (mem_counter < MEM_SIZE) {
      //ONLY STORE IF NOT A STOP
      if ((joystick_x > 1495 || joystick_x < 1505) && (joystick_y > 1495 || joystick_y < 1505)) {
         mem_x[mem_counter] = joystick_x;
         mem_y[mem_counter] = joystick_y;
         mem_counter++;
      }
    }
  } else if (mode == AUTO) {
 
    digitalWrite(MODE_LED, LOW);
    if (blueToothSerial.available()) { //does this need to be while?
      x = blueToothSerial.readBytesUntil('\n', input_buffer, BUF_SIZE);
      input_buffer[x-1]='\0';
      Serial.println(input_buffer);
      if (input_buffer[0] == 'f') {
         play_melody();
      } else if (input_buffer[0] == 'r') {
        Serial.println("REVERSING");
        while (mem_counter > 0) {
          blueToothSerial.print(1500 - mem_x[mem_counter]);
          blueToothSerial.print(' ');
          blueToothSerial.println(1500 + mem_y[mem_counter]);
          mem_counter--;
          Serial.println(mem_counter);
          delay(200);
        }
        blueToothSerial.println("m");
        mode = MANUAL;
      }
    }
  }
}

void play_melody() {
  int songLength = 26;
  int melody[] = {
    NOTE_FS4, NOTE_B4, NOTE_FS4, NOTE_B4, NOTE_D5, 0, NOTE_B4, 0,
    NOTE_D5, NOTE_B4, NOTE_D5,  NOTE_FS5, 0, NOTE_D5, 0,
    NOTE_FS5, NOTE_D5, NOTE_FS5, NOTE_A5, 0, NOTE_A4, 0,
    NOTE_D5, NOTE_A4, NOTE_D5, NOTE_FS5
  };
  int noteDurations[] = {
    8,8,8,8,4,8,4,8,
    8,8,8,4,8,4,8,
    8,8,8,4,8,4,8,
    8,8,8,2
  };

  // iterate over the notes of the melody:
  for (int i = 0; i < songLength; i++) {
    // to calculate the note duration, take one second divided by the note type.

    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / noteDurations[i];

    tone(BUZZER, melody[i], noteDuration);

    // to distinguish the notes, set a minimum time between them.

    // the note's duration + 30% seems to work well:

    int pauseBetweenNotes = noteDuration * 1.30;

    delay(pauseBetweenNotes);
    // stop the tone playing:
    noTone(8);
  }
}

void read_joystick() {
  //TO DO: CALLIBRATE
   joystick_x = -map(analogRead(JOY_X), 0, 1023, -200, 200);
   joystick_y = map(analogRead(JOY_Y), 0, 1023, -200, 200);
}

void setupBlueToothConnection() {
/*

  blueToothSerial.begin(9600);

  blueToothSerial.print("AT");
  delay(400);

  blueToothSerial.print("AT+DEFAULT");             // Restore all setup value to factory setup
  delay(4000);

  blueToothSerial.print("AT+NAMETimMaster");    // set the bluetooth name as "SeeedMaster" ,the length of bluetooth name must less than 12 characters.
  delay(400);

  blueToothSerial.print("AT+ROLEM");             // set the bluetooth work as master
  delay(400);


  //blueToothSerial.print("AT+AUTH1");
  //  delay(400);

  blueToothSerial.print("AT+CLEAR");             // Clear connected device mac address
  delay(400);

  blueToothSerial.flush();
*/

    Serial.println("Setting up the local (master) Bluetooth module.");

    masterNameCmd += shieldPairNumber;
    masterNameCmd += "\r\n";
    Serial.println(masterNameCmd);
    blueToothSerial.print("\r\n+STWMOD=1\r\n");      // Set the Bluetooth to work in master mode
    blueToothSerial.print(masterNameCmd);            // Set the bluetooth name using masterNameCmd
    blueToothSerial.print("\r\n+STAUTO=0\r\n");      // Auto-connection is forbidden here

    //  print() sets up a transmit/outgoing buffer for the string which is then transmitted via interrupts one character at a time.
    //  This allows the program to keep running, with the transmitting happening in the background.
    //  Serial.flush() does not empty this buffer, instead it pauses the program until all Serial.print()ing is done.
    //  This is useful if there is critical timing mixed in with Serial.print()s.
    //  To clear an "incoming" serial buffer, use while(Serial.available()){Serial.read();}

    blueToothSerial.flush();
    delay(2000);                                     // This delay is required

    blueToothSerial.print("\r\n+INQ=1\r\n");         // Make the master Bluetooth inquire

    blueToothSerial.flush();
    delay(2000);                                     // This delay is required

    Serial.println("Master is inquiring!");
}

void getSlaveAddress() {
  slaveName += shieldPairNumber;

  Serial.print("Searching for address of slave: ");
  Serial.println(slaveName);

  slaveName = ";" + slaveName;   // The ';' must be included for the search that follows

  char recvChar;

  // Initially, if(blueToothSerial.available()) will loop and, character-by-character, fill recvBuf to be:
  //    +STWMOD=1 followed by a blank line
  //    +STNA=MasterTest (followed by a blank line)
  //    +S
  //    OK (followed by a blank line)
  //    OK (followed by a blank line)
  //    OK (followed by a blank line)
  //    WORK:
  //
  // It will then, character-by-character, add the result of the first device that responds to the +INQ request:
  //    +RTINQ=64,A2,F9,xx,xx,xx;OnePlus 6 (xx substituted for anonymity)
  //
  // If this does not contain slaveName, the loop will continue. If nothing else responds to the +INQ request, the
  // process will appear to have frozen BUT IT HAS NOT. Be patient. Ask yourself why your slave has not been detected.
  // Eventually your slave will respond and the loop will add:
  //    +RTINQ=0,6A,8E,16,C4,1B;SlaveTest
  //
  // nameIndex will identify "SlaveTest", return a non -1 value and progress to the if() statement.
  // This will strip 0,6A,8E,16,C4,1B from the buffer, assign it to slaveAddr, and break from the loop.

  while(1) {
    if(blueToothSerial.available()) {
      recvChar = blueToothSerial.read();
      recvBuf += recvChar;

      nameIndex = recvBuf.indexOf(slaveName);   // Get the position of slave name

      if ( nameIndex != -1 ) {   // ie. if slaveName was found

        addrIndex = (recvBuf.indexOf(retSymb,(nameIndex - retSymb.length()- 18) ) + retSymb.length());   // Get the start position of slave address
        slaveAddr = recvBuf.substring(addrIndex, nameIndex);   // Get the string of slave address

        Serial.print("Slave address found: ");
        Serial.println(slaveAddr);

        break;  // Only breaks from while loop if slaveName is found
      }
    }
  }
}

void makeBlueToothConnection() {
  Serial.println("Initiating connection with slave.");
  char recvChar;

  // Having found the target slave address, now form the full connection command
  connectCmd += slaveAddr;
  connectCmd += "\r\n";

  int connectOK = 0;       // Flag used to indicate succesful connection
  int connectAttempt = 0;  // Counter to track number of connect attempts

  // Keep trying to connect to the slave until it is connected (using a do-while loop)

  do {
    Serial.print("Connect attempt: ");
    Serial.println(++connectAttempt);

    blueToothSerial.print(connectCmd);   // Send connection command

      // Initially, if(blueToothSerial.available()) will loop and, character-by-character, fill recvBuf to be:
      //    OK (followed by a blank line)
      //    +BTSTATE:3 (followed by a blank line)(BTSTATE:3 = Connecting)
      //
      // It will then, character-by-character, add the result of the connection request.
      // If that result is "CONNECT:OK", the while() loop will break and the do() loop will exit.
      // If that result is "CONNECT:FAIL", the while() loop will break with an appropriate "FAIL" message
      // and a new connection command will be issued for the same slave address.

    recvBuf = "";

    while(1) {
      if(blueToothSerial.available()) {
        recvChar = blueToothSerial.read();
        recvBuf += recvChar;

        if(recvBuf.indexOf("CONNECT:OK") != -1) {
          connectOK = 1;
          Serial.println("Connected to slave!");
          blueToothSerial.print("Master-Slave connection established!");
          break;
        } else if(recvBuf.indexOf("CONNECT:FAIL") != -1) {
          Serial.println("Connection FAIL, try again!");
          break;
        }
      }
    }
  }
  while (0 == connectOK);
}

//OLD CODe
    /*
    Serial.println("---");
    Serial.println(mode);
    Serial.print("X: ");
    Serial.println(joystick_x);
    Serial.print("Y: ");
    Serial.println(joystick_y);
    Serial.print("Z: ");


    if (joystick_z == 1) {
        automatic = !automatic;
        if (automatic)
            blueToothSerial.println("a");

            if (blueToothSerial.read() == 'r') {
              Serial.println("test");
            }
        else
        {
            blueToothSerial.println("m");
        }
       delay(200);
    } else if (!automatic) {
        blueToothSerial.print(1500 + joystick_x);
        blueToothSerial.print(' ');
        blueToothSerial.println(1500 - joystick_y);
        delay(200);

        if (mem_counter < MEM_SIZE) {
          mem_x[mem_counter] = joystick_x;
          mem_y[mem_counter] = joystick_y;
          mem_counter++;
        }
    }
}*/
