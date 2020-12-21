/*
  Reel Clock


  Pin plan
     DS3132
       SCL               = A5
       SDA               = A4
       Ground
       3.3v

     Reel Solenoid Control
       Reel Minutes      = D3
       Reel TenMinutes   = D6
       Reel Hours        = D8
       Reel TenHours     = D9

       Mutilplexer control
       S0                = A0
       S1                = A1

       Reel position input data
       Display0          = D10
       Display1          = D11
       Display2          = D12
       Display3          = D13

       LED pinout
       Tilt              = A2

*/

#include <Wire.h>                             // Needed for native DS3231 communication 
#include <DS3231.h>                           // Needed to set time
#include <OneButton.h>                        // To handle the buttons

#define BUTTON_PIN_1 2                        // Define button one right side
#define BUTTON_PIN_2 3                        // Define button two left side

#define Reel_1 0                              // Define reel number 1 first digit of minute or day or year
#define Reel_2 1                              // Define reel number 2 second digit of minute or day or year
#define Reel_3 2                              // Define reel number 3 first digit of hours or month 3rd digit of year
#define Reel_4 3                              // Define reel number 4 second digit of hour or month 4rd digit of year

#define humanDelay    1000                    // define how long a display should hold. Used in date/year display
#define reeldelay     50                      // for 50th of a second reel pick duration
#define nextMoveDelay 200                     // Delay after moving solenoid

#define tilt        1                         // Used to controlling the LEDs
#define extraGame   2                         // Used to controlling the LEDs
#define player1     3                         // Used to controlling the LEDs
#define player2     4                         // Used to controlling the LEDs

#define LEDOn       1                         // LED state control
#define LEDOff      2                         // LED state control

#define LEDTILT A2                            // Tilt LED

#define minuteReel    4                       // Ping out for solenoid control - Lower minute
#define tenminuteReel 6                       // Ping out for solenoid control - Upper minute
#define lowerHourReel 8                       // Ping out for solenoid control - Lower hour
#define upperHourReel 9                       // Ping out for solenoid control - Upper hour

#define display0  10                          // Input pins for BCD from reel bit 1
#define display1  11                          // Input pins for BCD from reel bit 2
#define display2  12                          // Input pins for BCD from reel bit 4
#define display3  13                          // Input pins for BCD from reel bit 8

#define DisplayControl_0 A0                    // Reel Display Control Pins lower bit
#define DisplayControl_1 A1                    // Reel Display Control Pins upper bit

#define ledTiltpin A2                         // LED output pin 

// DS3231 rtc;                                   // create time object
// DateTime tt;                                  // variable of type DateTime

//Bounce debouncer1 = Bounce();                 // Instantiate a Bounce object
//Bounce debouncer2 = Bounce();                 // Instantiate second Bounce object

volatile int debounceTime = 10;               // change as needed to get the switch to work once per push

volatile int tdisplay;                        // used for the pcb data

volatile boolean  clock_12 = true;            // True for 12 hour clock; false for 24
volatile boolean  day_light_saving = false;   // True for Day Light Saving Time; false for the rest of the year
volatile boolean  PM = false;                 // State of AM or PM

volatile boolean  DEBUG = false;              // used for debugging

volatile byte     sethour;                    // used to hold hour value in time calculations

volatile int Reel_1_value;
volatile int Reel_2_value;
volatile int Reel_3_value;
volatile int Reel_4_value;

volatile byte lowerminute;
volatile byte upperminute;
volatile byte lowerhour;
volatile byte upperhour;

int reeldisplay0 = 0;             // Current display number 4 bit code
int reeldisplay1 = 0;             // Current display number
int reeldisplay2 = 0;             // Current display number
int reeldisplay3 = 0;             // Current display number

//***************************************************
//
// Begin Setup
//
//***************************************************

void setup() {

   rtc.begin();

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(__DATE__, __TIME__));
  }

  
  //
  // Have to ensure DS3231 is set to continue the clock if it was powered off
  //
  Wire.beginTransmission(0x68);     // address DS3231
  Wire.write(0x0E);                 // select register
  Wire.write(0b00011100);           // write register bitmap,
  // bit 7 is Enable Oscillator (/EOSC) must be 0 in
  //order to keep time while powered off
  //
  // bit 6 Battery-Backed Square-Wave Enable (BBSQW) is 0
  // bit 5 Convert Temperature (CONV) is 0
  // bit 4 & 3 Rate Select (RS2 and RS1) is 1 & 1 square-wave output = 8.192kHz
  // bit 2 Interrupt Control (INTCN) is 1
  // Bit 1 & 0 Alarm 2 Interrupt Enable (A2IE) & Alarm 1 Interrupt Enable (A1IE) both off
  Wire.endTransmission();           // clear buffer

  //
  //  LED defines - not done
  //

  pinMode(ledTiltpin, OUTPUT);

  //
  // define reel control output pins
  //

  pinMode(minuteReel,     OUTPUT);
  pinMode(tenminuteReel,  OUTPUT);
  pinMode(lowerHourReel,  OUTPUT);
  pinMode(upperHourReel,  OUTPUT);

  //
  // set all Reel Solenoid off
  //

  digitalWrite(minuteReel,    HIGH);
  digitalWrite(tenminuteReel, HIGH);
  digitalWrite(lowerHourReel, HIGH);
  digitalWrite(upperHourReel, HIGH);

  //
  // Reel select control pins for reading reel value
  //
  //          00 = reel for lower minutes
  //          01 = reel for upper minutes
  //          10 = reel for lower hours
  //          11 = reel for upper hours
  //

  pinMode(DisplayControl_0, OUTPUT);
  pinMode(DisplayControl_1, OUTPUT);

  //
  // setup up vaalues
  //

  digitalWrite(DisplayControl_0, LOW);
  digitalWrite(DisplayControl_1, LOW);

  //
  // Reel display Input pins
  //

  pinMode(display0, INPUT);
  pinMode(display1, INPUT);
  pinMode(display2, INPUT);
  pinMode(display3, INPUT);

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }


    if (!rtc.begin()) {
      Serial.println("Couldn't find RTC");
      while (1);   // sketch halts in an endless loop
    }

  // Check to see if RTC is running
  //  if (! rtc.isrunning()) {
  //    Serial.println("RTC is NOT running!");
  //  }

  // If debug mode active set RTC time to CPU time
  if (DEBUG) {
    //    rtc.adjust(DateTime(__DATE__, __TIME__));
    Serial.print ("Set time to RTC Chip");
    Serial.println();
    Serial.println();
    Serial.println(__TIME__);
    Serial.println(__DATE__);
    DEBUG = false;
  }
}
//
//end setup
//

void LEDcontrol (int LED, int state) {
  /*
     Valid LED values are:
        tilt        Used for PM indicator
        extraGame
        player1     Used for Date
        player2     Used for Year

     Valid States are:
      On
      Off
  */
  if (LED == tilt && state == LEDOn) {
    digitalWrite(ledTiltpin, HIGH);        // turn the LED on (HIGH is the voltage level)
  }
  else {
    digitalWrite(ledTiltpin, LOW);         // turn the LED off by making the voltage LOW
  }
  //  if (LED == extraGame && state == ledOn) {
  //      digitalWrite(ledTiltpin, HIGH);         // turn the LED off by making the voltage LOW
  //      }
}

int getdisplayvalue (int x) {
  // Set the control lines for the reel input
  // x = 0 for minute
  // x = 1 for Ten Minute
  // x = 2 for Hour
  // x = 3 for Ten Hour

  tdisplay = 0;  // intialize return variabile

  switch (x) {
    case 0: {
        digitalWrite(DisplayControl_0, LOW);
        digitalWrite(DisplayControl_1, LOW);
      }
      break;
    case 1: {
        digitalWrite(DisplayControl_0, HIGH);
        digitalWrite(DisplayControl_1, LOW);
      }
      break;
    case 2: {
        digitalWrite(DisplayControl_0, LOW);
        digitalWrite(DisplayControl_1, HIGH);
      }
      break;
    case 3: {
        digitalWrite(DisplayControl_0, HIGH);
        digitalWrite(DisplayControl_1, HIGH);
      }
      break;
  }                                                 // end of switch statement

  reeldisplay0 = digitalRead (display0);            // read the reel value
  reeldisplay1 = digitalRead (display1);
  reeldisplay2 = digitalRead (display2);
  reeldisplay3 = digitalRead (display3);

  if (reeldisplay0 == 0) tdisplay += 1;             // building the answer
  if (reeldisplay1 == 0) tdisplay += 2;             // data is inverted therefore 0 is really a 1
  if (reeldisplay2 == 0) tdisplay += 4;
  if (reeldisplay3 == 0) tdisplay += 8;
  return (tdisplay);                                // return display value 0 to 9
}                                                 // end of procedure getdisplayvalue()


void setreel (int rNumber, int DisplayValue) {
  switch (rNumber) {
    //set lower minute reel
    case 0: {
        Reel_1_value = getdisplayvalue(rNumber);
        lowerminute =  DisplayValue % 10;
        if (Reel_1_value == lowerminute) break;
        while (Reel_1_value != lowerminute) {
          digitalWrite(minuteReel, LOW);        // sets the Minute Reel on
          delay(reeldelay);                     // waits for defined delay
          digitalWrite(minuteReel, HIGH);       // sets the MInute Reel off
          delay(nextMoveDelay);
          Reel_1_value = getdisplayvalue(rNumber);
        }
      }
      break;

    case 1: {
        Reel_2_value = getdisplayvalue(rNumber);
        upperminute   = (DisplayValue / 10);
        if (Reel_2_value == upperminute) break;
        while (Reel_2_value != upperminute) {
          digitalWrite(tenminuteReel, LOW);       // sets the lower Minute Reel on
          delay(reeldelay);                       // waits for defined delay
          digitalWrite(tenminuteReel, HIGH);      // sets the ten Minute Reel off
          delay(nextMoveDelay);                   // delay before moving reel again
          Reel_2_value = getdisplayvalue(rNumber);
        }
      }
      break;

    case 2: {
        Reel_3_value = getdisplayvalue(rNumber);
        lowerhour   = (DisplayValue % 10);
        if (Reel_3_value == lowerhour) break;
        while (Reel_3_value != lowerhour) {
          digitalWrite(lowerHourReel, LOW);       // sets the Lower Hour Reel on
          delay(reeldelay);                       // waits for defined delay
          digitalWrite(lowerHourReel, HIGH );     // sets the Lower Hour Reel off
          delay(nextMoveDelay);                   // Allows numbers to be read from the reels better
          Reel_3_value = getdisplayvalue(rNumber);
        }
      }                                              // end of case 2
      break;                                         // Break from case 2

    case 3: {
        Reel_4_value = getdisplayvalue(rNumber);
        upperhour = (DisplayValue / 10);
        if (Reel_4_value == upperhour) break;
        while (Reel_4_value != upperhour) {
          digitalWrite(upperHourReel, LOW);       // sets the Minute Reel on
          delay(reeldelay);                       // waits for defined delay
          digitalWrite(upperHourReel, HIGH);      // sets the Minute Reel off
          delay(nextMoveDelay);
          Reel_4_value = getdisplayvalue(rNumber);
        }                                     // end of while
      }                                       // end of Case 3
      break;                                // break from case statement
  }                                         // end of switch statement
}                                           // end of setreel procedure


void displayDate () {
  Serial.println ("");
  Serial.println ("displayDate called");
  Serial.println ("");
  unsigned int tempYear = 0;

  Serial.print ("Day = ");
  Serial.println (tt.day(), DEC);
  setreel (Reel_1, tt.day());             // set day reel 1
  setreel (Reel_2, tt.day());             // set day reel 2
  setreel (Reel_3, tt.month());           // set month reel
  setreel (Reel_4, tt.month());           // set month reel
  LEDcontrol(tilt, LEDOn);
  Serial.println("");
  Serial.println("Month/day set");
  Serial.println("");
  delay (humanDelay);                    // delay so date can be read
  LEDcontrol(tilt, LEDOff);

  Serial.println(tt.year());
  tempYear = tt.year() - 2000;            // Get lower byte of 16-bit var
  Serial.println (tempYear, DEC);
  setreel (Reel_1, tempYear);        // set year reel 1
  setreel (Reel_2, tempYear);        // set year reel 2
  setreel (Reel_3, 0);               // set year reel hard coded 0
  setreel (Reel_4, 2);               // set year reel hard coded 2
  delay (humanDelay);                // delay so year can be read
  Serial.println("Done with Set day,month & year");
}

void loop() {
  DateTime now = RTC.now();

  tt = rtc.now();                   // get current time & date

  setreel (Reel_1, tt.minute());    // set minute reel
  setreel (Reel_2, tt.minute());    // set second minute reel

  sethour = tt.hour();              // Have to deal with 12/24 hour setups
  if (clock_12 && sethour == 12) {
    PM = true;
    LEDcontrol (tilt, LEDOn);      // PM indicator
  }
  if (clock_12 && sethour == 00) {
    sethour = 12;
    PM = false;
    LEDcontrol (tilt, LEDOff);      // PM indicator
  }
  if (clock_12  && (sethour > 12)) {
    sethour = sethour - 12;
    PM = true;
    LEDcontrol (tilt, LEDOn);      // PM indicator
  }
  setreel (Reel_3, sethour);     // set hour reel
  setreel (Reel_4, sethour);     // set seoncd hour reel
  //
  // LED control
  //
  if (PM) {
    LEDcontrol(tilt, LEDOn);
  }
  else {
    LEDcontrol (tilt, LEDOff);
  }
}
/*
  if (DEBUG3) {
  Serial.print ("EEPROM position 3 = ");
  Serial.print (EEPROM.read(3));
  Serial.println();
  }
  // isSwitchs();
  Serial.print ("\t");
  Serial.print ("\t");
  Serial.print ("\t");
  Serial.print ("\t");
  Serial.println();

  Serial.print (getdisplayvalue(0));
  Serial.print ("\t");
  Serial.print (getdisplayvalue(1));
  Serial.print ("\t");
  Serial.print (getdisplayvalue(2));
  Serial.print ("\t");
  Serial.print (getdisplayvalue(3));
  Serial.print ("\t");
  delay (3000);
  Serial.println();
  Serial.print ("Calling flashLED  -  ");
  flashLED (tilt, ledBlink);
  Serial.println ("Back flashLED");
  delay (5000);
  flashLED (tilt, ledOff);
  delay(1000);
  }
*/
