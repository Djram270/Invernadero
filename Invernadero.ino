/**
\file   sketch.ino
\date   2023-05-12
author Samuel Guilombo <samuelguilombo@unicauca.edu.co> - Oscar Tosne <oscartosne@unicauca.edu.co> - Steven Chanchi <schanchi@unicauca.edu.co>
@brief  Smart greenhouse Program.

\par Copyright
Information contained here in is proprietary to and constitutes valuable
confidential trade secrets of Unicauca, and
is subject to restrictions on use and disclosure.

\par
Copyright (c) Unicauca 2023. All rights reserved.

\par
The copyright notices above do not evidence any actual or
intended publication of this material.
******************************************************************************
*/

#include "StateMachineLib.h"    /**< Include for the state machine library. */
#include "AsyncTaskLib.h"       /**< Include for asynchronous task library. */
#include <LiquidCrystal.h>      /**< Include for the LiquidCrystal library. */
#include <Keypad.h>             /**< Include for the keypad library. */
#include "DHT.h"                /**< Include for the DHT sensor library. */
#define DHTPIN A1              /**< Define for the pin connected to DHT sensor. */
#define DHTTYPE DHT11         /**< Define for the type of DHT sensor (DHT22 or DHT11). */
#include <AverageValue.h>       /**< Include for the average value computation library. */
#define PHOTOCELL_PIN A0       /**< Define for the pin connected to the photocell sensor. */
#define BUZZER 6               /**< Define for the pin connected to the buzzer. */

/** @brief Variable to store the starting time. */
unsigned int startTime = 0;

/** @brief Variable to store the current time. */
unsigned int actualTime = 0;

/** @brief LCD object with pin configuration. */
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

/** @brief Variable to store light intensity. */
short int luz = 0.0;

// RGB config constants
const int redPin = 7;    /**< Pin for the red component of the RGB LED. */
const int greenPin = 8;  /**< Pin for the green component of the RGB LED. */
const int bluePin = 9;   /**< Pin for the blue component of the RGB LED. */

// PHOTO constants
const float GAMMA = 0.7;  /**< Gamma value for photo-related calculations. */
const float RL10 = 50;    /**< Resistance value for photo-related calculations. */

void tiempoActual(void);
int tiempo = 0;

// RGB functions
/**
 * @brief Sets the color of the RGB LED.
 * @param red Value for the red component.
 * @param green Value for the green component.
 * @param blue Value for the blue component.
 */
void color(unsigned char red, unsigned char green, unsigned char blue) {
    analogWrite(redPin, red);
    analogWrite(bluePin, blue);
    analogWrite(greenPin, green);
}

// Init variables for password
char password[] = {'1', '2', '3', '4'}; /**< Predefined password. */
int passwordVerify[4];                  /**< Array to store entered password digits. */
int numLetter = 0;                      /**< Counter for entered password digits. */
int gblCountMistakes = 0;               /**< Counter for password entry mistakes. */
int countCorrect = 0;                   /**< Counter for correct password entries. */

// Keypad config
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {24, 26, 28, 30}; 
byte colPins[COLS] = {32, 34, 36, 38};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Create new StateMachine
StateMachine stateMachine(5, 8); /**< StateMachine instance with pin configuration. */
  
// State Alias
/**
 * @brief Enumeration of different states for the state machine.
 */
enum State {
    StateInit = 0,         /**< Initial state. */
    StateBlocked = 1,      /**< State indicating a blocked condition. */
    StateMonitorTH = 2,    /**< State for monitoring temperature and humidity. */
    StateAlarm = 3,        /**< State indicating an alarm condition. */
    StateMonitorLight = 4, /**< State for monitoring light intensity. */
};
// Input Alias
enum Input
{
    InInit = 0,         /**< Input indicating initialization. */
    InBlocked = 1,      /**< Input indicating a blocked state. */
    InMonitorTH = 2,    /**< Input for monitoring temperature and humidity. */
    InAlarm = 3,        /**< Input indicating an alarm condition. */
    InLight = 4,        /**< Input for monitoring light intensity. */
    TimeOut5 = 5,       /**< Time-out input for 5 seconds. */
    TimeOut3 = 6,       /**< Time-out input for 3 seconds. */
    TimeOut6 = 7        /**< Time-out input for 6 seconds. */
};

// Stores last user input
Input input;  /**< Variable to store the last user input. */

// Tasks
AsyncTask timeOutTask5(5000, false, []() { fnTimeOut5(); });  /**< Task for handling 5-second time-outs. */
AsyncTask timeOutTask3(3000, false, []() { fnTimeOut3(); });  /**< Task for handling 3-second time-outs. */
AsyncTask timeOutTask6(6000, false, []() { fnTimeOut6(); });  /**< Task for handling 6-second time-outs. */
AsyncTask initTask(50, true, []() { fnInit(); });            /**< Initialization task. */
AsyncTask monitorTHTask(200, true, []() { fnMonitorTH(); });   /**< Task for monitoring temperature and humidity. */
AsyncTask monitorLuzTask(200, true, []() { fnMonitorLuz(); });/**< Task for monitoring light intensity. */
AsyncTask alarmaTask(200, true, []() { fnAlarma(); });         /**< Alarm task. */

/**
 * @brief Function handling the 5-second time-out.
 */
void fnTimeOut5()
{
    input = TimeOut5; /**< Assigns the 5-second time-out as the last user input. */
}

/**
 * @brief Function handling the 3-second time-out.
 */
void fnTimeOut3()
{
    input = TimeOut3; /**< Assigns the 3-second time-out as the last user input. */
}

/**
 * @brief Function handling the 6-second time-out.
 */
void fnTimeOut6()
{
    input = TimeOut6; /**< Assigns the 6-second time-out as the last user input. */
}

/**
 * @brief Function to start the initialization task.
 */
void inputInit()
{
    initTask.Start();
}

DHT dht(DHTPIN, DHTTYPE); /**< DHT sensor initialization. */
const long MAX_VALUES_NUM_H = 25; /**< Maximum number of values for averaging. */
AverageValue<long> averageValueT(MAX_VALUES_NUM_H); /**< Average value computation for temperature. */
AverageValue<long> averageValueH(MAX_VALUES_NUM_H); /**< Average value computation for humidity. */
AverageValue<long> cleanAverageValue(MAX_VALUES_NUM_H); /**< Average value computation for cleaning purposes. */

/**
 * @brief Function to start the task monitoring temperature and humidity.
 */
void inputMonitorTH()
{
  noTone(BUZZER);
  color(0, 0, 0);
  startTime = millis();
  lcd.clear();
  countCorrect = 0;
  averageValueT = cleanAverageValue;
  averageValueH = cleanAverageValue;
  numLetter = 0;
  dht.begin();
  timeOutTask5.Start();
  monitorTHTask.Start();
}

const long MAX_VALUES_NUM_L = 15; /**< Maximum number of values for averaging. */
AverageValue<long> averageValueLuz(MAX_VALUES_NUM_L); /**< Average value computation for light intensity. */

/**
 * @brief Function to start the task monitoring light intensity.
 */
void inputMonitorLuz()
{
  startTime = millis();
  lcd.clear();
  numLetter = 0;
  averageValueLuz = cleanAverageValue;
  timeOutTask3.Start();
  monitorLuzTask.Start();
}
/**
 * @brief Function to start the alarm task.
 */
void inputAlarma()
{
  lcd.clear();
  color(255, 0, 0);
  timeOutTask6.Start();
  alarmaTask.Start();
}

/**
 * @brief Function to stop the initialization output task.
 */
void outputInit()
{
    initTask.Stop();
}

/**
 * @brief Function to stop the task monitoring temperature and humidity output.
 */
void outputMonitorTH()
{
  //timeOutTask3.Start();
  monitorTHTask.Stop();
}

/**
 * @brief Function to stop the task monitoring light intensity output.
 */
void outputMonitorLuz()
{
  //timeOutTask5.Start();
  monitorLuzTask.Stop();
}

/**
 * @brief Function to stop the alarm output task.
 */
void outputAlarma()
{
    alarmaTask.Stop();
}

/**
 * @brief Function to set up the State Machine with transitions and actions.
 */
void setupStateMachine()
{
    // Add transitions
    stateMachine.AddTransition(StateInit, StateBlocked, []() { return input == InBlocked; });
    stateMachine.AddTransition(StateInit, StateMonitorTH, []() { return input == InMonitorTH; });
    stateMachine.AddTransition(StateBlocked, StateInit, []() { return input == TimeOut5; });
    stateMachine.AddTransition(StateMonitorTH, StateAlarm, []() { return input == InAlarm; });
    stateMachine.AddTransition(StateMonitorTH, StateMonitorLight, []() { return input == TimeOut5; });
    stateMachine.AddTransition(StateAlarm, StateMonitorTH, []() { return input == TimeOut6; });
    stateMachine.AddTransition(StateMonitorLight, StateAlarm, []() { return input == InAlarm; });
    stateMachine.AddTransition(StateMonitorLight, StateMonitorTH, []() { return input == TimeOut3; });

    // Add actions
    stateMachine.SetOnEntering(StateInit, inputInit);
    stateMachine.SetOnEntering(StateBlocked, inputBlocked);
    stateMachine.SetOnEntering(StateMonitorTH, inputMonitorTH);
    stateMachine.SetOnEntering(StateAlarm, inputAlarma);
    stateMachine.SetOnEntering(StateMonitorLight, inputMonitorLuz);
    stateMachine.SetOnLeaving(StateInit, []() { outputInit(); });
    stateMachine.SetOnLeaving(StateMonitorTH, []() { outputMonitorTH(); });
    stateMachine.SetOnLeaving(StateAlarm, []() { outputAlarma(); });
    stateMachine.SetOnLeaving(StateMonitorLight, []() { outputMonitorLuz(); });
}
/**
 * @brief Initialization function called once at the beginning of the program.
 */
void setup() 
{
    // Setup LCD
    lcd.begin(16, 2);
    
    // Setup for RGB
    pinMode(redPin, OUTPUT); 
    pinMode(greenPin, OUTPUT);
    pinMode(bluePin, OUTPUT);
    pinMode(BUZZER, OUTPUT); 

    // Setup for State machine
    Serial.begin(9600);
    setupStateMachine();  

    // Initial state
    stateMachine.SetState(StateInit, false, true);
}

/**
 * @brief Main execution loop of the program.
 */
void loop() 
{
    monitorLuzTask.Update();
    timeOutTask5.Update();
    timeOutTask3.Update();
    timeOutTask6.Update();
    alarmaTask.Update();
    initTask.Update();
    monitorTHTask.Update();
    stateMachine.Update();  // Remove if causing issues
}

// Functions for password

/**
 * @brief Function called when access is granted after correct password entry.
 */
void accessGranted(){
    lcd.clear();
    color(0, 255, 0);
    lcd.print("Correcto");
    delay(800);
    lcd.clear();
    color(0, 0, 0);
    input = InMonitorTH;
}

/**
 * @brief Function called when an incorrect password is entered.
 */
void incorrectPassword(){
    lcd.clear();
    if(gblCountMistakes < 2){
        color(0, 0, 255);
        lcd.print("Incorrecto");
        delay(500);
        gblCountMistakes++;
        lcd.clear();
        lcd.print("Password: ");
    }
    else{
        countCorrect = -1;
        input = InBlocked;
    }
}
/**
 * @brief Function to initialize and manage password input.
 */
void fnInit()
{
    lcd.clear();
    numLetter = 0;
    countCorrect = 0;
    gblCountMistakes = 0;
    lcd.print("Password: ");
    do {
        color(0, 0, 0);
        char key = keypad.getKey();
        if (key) {
            lcd.print('*');
            passwordVerify[numLetter] = key;
            numLetter++;
            delay(100);
        }
        if (numLetter == 4) {
            countCorrect = 0;
            for (int i = 0; i < numLetter; i++) {
                if (password[i] == passwordVerify[i]) {
                    countCorrect++;
                }
            }
            if (countCorrect == numLetter) {
                accessGranted();
                numLetter = 0;
            } else {
                incorrectPassword();
                numLetter = 0;
            }
        }
    } while ((countCorrect != -1) && (countCorrect != 4));
}

/**
 * @brief Function called when the input is blocked.
 */
void inputBlocked()
{
    color(255, 0, 0);
    lcd.print("Bloqueado");
    timeOutTask5.SetIntervalMillis(5000);
    timeOutTask5.Start();
}

/**
 * @brief Function to monitor temperature and humidity.
 */
void fnMonitorTH()
{

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float f = dht.readTemperature(true);

  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  float hif = dht.computeHeatIndex(f, h);
  float hic = dht.computeHeatIndex(t, h, false);

  lcd.setCursor(0, 0);
  lcd.print("Humidity: ");
  lcd.print(h);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("Temp: ");
  lcd.print(t);
  lcd.print("C");
  averageValueT.push(t);
  averageValueH.push(h);

  if (t > 30 && h > 70)
  {
    input = InAlarm;
    return;
  }
}

/**
 * @brief Function to handle the alarm state.
 */
void fnAlarma() {
  //if ((millis() - tiempo ) >= 6000)
  //{
  //  input = TimeOut6;
  //  return;
  //}
  lcd.print("Alarma");
  tone(BUZZER, 1000, 5000);
}

/**
 * @brief Function to monitor light intensity.
 */
void fnMonitorLuz()
{  
  lcd.setCursor(0, 0);
  int analogValue = analogRead(PHOTOCELL_PIN);
  lcd.print("Luz: ");
  lcd.print(analogValue);
  averageValueLuz.push(analogValue);

  if (analogValue < 20.0) 
  {
    input = InAlarm;
    return;
  }
}
