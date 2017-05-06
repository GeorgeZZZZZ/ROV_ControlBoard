#include <ESP8266WiFi.h>
#define HOSTNAME "ControlBoard" // useless
#define MAX_SRV_CLIENTS 1
#define MAX_Buff_Length 5

typedef struct
{
  String result;
  bool isEnd;
  bool err;
}ProcessContentResult;
ProcessContentResult PCResult;

typedef struct
{
  byte result;
  bool isEnd;
  bool err;
}ProcessEndingResult;
ProcessEndingResult PEResult;

const char* ssid = "CommStation";
const char* password = "qwertyui";

IPAddress IP(10,0,0,120); 
IPAddress gateway(10,0,0,1);
IPAddress subnet(255,255,255,0);

WiFiServer server(23);
WiFiClient serverClients[MAX_SRV_CLIENTS];
/*
const char* testServerIP = "10.0.0.100";
int testPort = 23;
WiFiClient client;
bool bConnected = false;
*/

// for state machine, 'const' make the variable read only
const uint8_t sm_pi_begin = -10;
uint8_t sm_pi_state = sm_pi_begin; //  sate machine for process incomming data
const uint8_t sm_pi_headerAnalyse = 0; // state machine processing incoming data, analyse the header
const uint8_t sm_pi_clearBeforeGoBack = 1; // state machine processing incoming data, analyse the header
const uint8_t sm_pi_error = 5;
//const uint8_t sm_pi_headerExtract = 10; // may be is wrong code, no need for contentEnding
const uint8_t sm_pi_fullEnding = 20;
const uint8_t sm_pi_EndingStage_0 = 21;
const uint8_t sm_pi_EndingStage_1 = 22;
const uint8_t sm_pi_leftMotor= 30;
const uint8_t sm_pi_rightMotor = 40;
const uint8_t sm_pi_midMotor = 50;
const uint8_t sm_pi_speed = 60;

// state machine left motor
uint8_t sm_lm_state = 0;
const uint8_t sm_lm_begin = 0;
const uint8_t sm_lm_delay_0 = 5;
const uint8_t sm_lm_delay_1 = 7;
const uint8_t sm_lm_positive = 10;
const uint8_t sm_lm_delay_2 = 15;
const uint8_t sm_lm_delay_3 = 17;
const uint8_t sm_lm_negative = 20;

// state machine right motor
uint8_t sm_rm_state = 0;
const uint8_t sm_rm_begin = 0;
const uint8_t sm_rm_delay_0 = 5;
const uint8_t sm_rm_delay_1 = 7;
const uint8_t sm_rm_positive = 10;
const uint8_t sm_rm_delay_2 = 15;
const uint8_t sm_rm_delay_3 = 17;
const uint8_t sm_rm_negative = 20;

// state machine middle motor
uint8_t sm_mm_state = 0;
const uint8_t sm_mm_begin = 0;
const uint8_t sm_mm_delay_0 = 5;
const uint8_t sm_mm_delay_1 = 7;
const uint8_t sm_mm_positive = 10;
const uint8_t sm_mm_delay_2 = 15;
const uint8_t sm_mm_delay_3 = 17;
const uint8_t sm_mm_negative = 20;

char pi_buff[MAX_Buff_Length];
uint8_t pi_bufferCount = 0;

int speedCommand = 0;  // can not use uint8_t, because will get incorrect number when trying to return negative uint8_t value from function
int leftMotorCommand = 0;
int rightMotorCommand = 0;
int midMotorCommand = 0;

int speedTemp = -999;
int leftMotorTemp = -999;
int rightMotorTemp = -999;
int midMotorTemp = -999;

bool leftMotorUpdateFlag = false;
bool rightMotorUpdateFlag = false;
bool midMotorUpdateFlag = false;

unsigned long leftMotorTimer = 0;
unsigned long rightMotorTimer = 0;
unsigned long midMotorTimer = 0;
unsigned long ledOffTimer = 0;
unsigned long ledOnTimer = 0;

unsigned long LMRTimer = 0; // left motor reverse timer,use to delay after reverse command come
unsigned long RMRTimer = 0; // right motor reverse timer
unsigned long MMRTimer = 0; // middle motor reverse timer

bool leftMotorReverseFlag = false;
bool rightMotorReverseFlag = false;
bool middleMotorReverseFlag = false;

/*
uint8_t lMotorPin = 0;  // pin 0, ESPDuino ESP-13
uint8_t rMotorPin = 2;  // pin 2
uint8_t mMotorPin = 4;
*/
///*
uint8_t lMotorPin = 5;  // pin 1, NodeMCU ESP-12E
uint8_t rMotorPin = 4;  // pin 2
uint8_t mMotorPin = 0;  // pin 3
uint8_t camMotorPin = 2;  // pin 4
uint8_t lMotorReversePin = 14;  // pin 5
uint8_t rMotorReversePin = 12;  // pin 6
uint8_t mMotorReversePin = 13;  // pin 7
uint8_t tfRecordPin = 15;  // pin 8
//*/

void setup() {
  pinMode(LED_BUILTIN, OUTPUT); // initiallize LED
  digitalWrite(LED_BUILTIN, LOW); // keep the LED on to indicate is in initializing step
  
  Serial.begin(115200); // initiallize serial for debug
  Serial.setDebugOutput(true);

  WiFi.config(IP, gateway, subnet); // static IP address
  WiFi.hostname("MyESPName");
  
  //WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); // start connect router
  
  Serial.println("\nConnecting to router");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  
  server.begin(); // start to run TCP server
  server.setNoDelay(true);
  
  analogWriteFreq(60);  // set PWM frequence as 60hz
  pinMode(lMotorPin, OUTPUT); // initialize pin out is necessary, althought don't initialize still can use pwm function but that require pwm out put low and high to work
  pinMode(rMotorPin, OUTPUT); // if no initialization and directly use pwm to control will not get correct value
  pinMode(mMotorPin, OUTPUT);
  
  pinMode(camMotorPin, OUTPUT);
  
  pinMode(lMotorReversePin, OUTPUT);
  pinMode(rMotorReversePin, OUTPUT);
  pinMode(mMotorReversePin, OUTPUT);
  pinMode(tfRecordPin, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH); // turn LED off indicate initializing step is over
}

void loop() {

  LEDManager(); // manage LED
  
  uint8_t i;  // "uint8_t" its shorthand for: a type of unsigned integer of length 8 bits
  
  // if client is connected in
  if (server.hasClient())
  {
    // Looking for new client connection
    for(i = 0; i < MAX_SRV_CLIENTS; i++)
    {
      if (!serverClients[i] || !serverClients[i].connected())
      {
        if(serverClients[i]) serverClients[i].stop();
        serverClients[i] = server.available();
        Serial.print("New client: "); Serial.println(i);
        continue;
      }
    }
    WiFiClient serverClient = server.available();
    serverClient.stop();  // why immediately close client?
  }

  // check process incoming message, look if its correct format
  // correct format of all three motor move 100% speed, should be: (L)On;(R)On;(M)On;(SP)100;[CR][LF]
  // correct format of only middle motor move 50% speed, should be: (M)On;(SP)50;[CR][LF]
  for(i = 0; i < MAX_SRV_CLIENTS; i++)
  {
    if (serverClients[i] && serverClients[i].connected())
    {
      Socket_Communication (i); // monitor cache for incoming data and process it 
    }
  }
  
  MotorCommandAutoReset (); // auto reset motor command if motor not receiving command any more
  
  MotorControl ();  // move motor

  //DebugAndTest ();
}

void LEDManager ()
{
  int offResetTime = 1900; // the time for led to turn off
  int onResetTime = 100;  // the time for led to turn on
  
  if (ledOffTimer == 0) ledOffTimer = millis(); // start countting led turnning off time
  if (TimeCounter (ledOffTimer, offResetTime))  // time is over
  {
    digitalWrite(LED_BUILTIN, LOW); // turn on LED
    if (ledOnTimer == 0)  ledOnTimer = millis();  // start countting led turnning on time
    if (TimeCounter (ledOnTimer, onResetTime))  // timer is over
    {
      digitalWrite(LED_BUILTIN, HIGH); // turn off LED
      ledOnTimer = ledOffTimer = 0; // reset timer
    }
  }
}

void Socket_Communication (int _clientNum)
{
  while(serverClients[_clientNum].available()) // loop when there are data coming
  {
    char _num;
    switch (sm_pi_state)
    {
      case sm_pi_begin:
        _num = serverClients[_clientNum].read();
        if (_num == '(')  sm_pi_state = sm_pi_headerAnalyse;
        else if (_num == '[') sm_pi_state = sm_pi_fullEnding;
        else
        {
          sm_pi_state = sm_pi_error;  // uncorrenct format data
          Serial.print("PI State Machine error: sm_pi_state");
        }
        //Serial.print("state change: "); // keep this code for future debug
        //Serial.println(sm_pi_state);
      break;
      
      case sm_pi_headerAnalyse: // analyse the header to understander what command comes
        ProcessIncoming_ContentExtract (_clientNum);
        if (PCResult.isEnd && !PEResult.isEnd && !PCResult.err) // when tacking header should not receive ending mark
        {
          String _header = PCResult.result; // extract header from incoming message
  
          if (_header == "L")  sm_pi_state = sm_pi_leftMotor;
          else if (_header == "R")  sm_pi_state = sm_pi_rightMotor;
          else if (_header == "M")  sm_pi_state = sm_pi_midMotor;
          else if (_header == "SP")  sm_pi_state = sm_pi_speed;
          else
          {
            sm_pi_state = sm_pi_error;  // uncorrenct format data
            Serial.println("PI State Machine error: sm_pi_headerAnalyse - 02.");
          }
        }
        else if (PCResult.err || PEResult.isEnd)
        {
          sm_pi_state = sm_pi_error;
          Serial.println("PI State Machine error: sm_pi_headerAnalyse - 01.");
        }
      break;
      
      case sm_pi_clearBeforeGoBack: // use to reset something
        CleanPISturct ();  //reset structure
        sm_pi_state = sm_pi_begin;
      break;
      
      case sm_pi_error: // if is not correct format or other error
        serverClients[_clientNum].flush();  // flush cache
        ResetMotorFlag ();
        CleanCommandTemp (); // reset motor command temp
        CleanPISturct ();  //reset structures
        sm_pi_state = sm_pi_begin;
        Serial.println("PI State Machine error: Error occured, Format not correct.");
      break;
      
      case sm_pi_fullEnding:
        ProcessIncoming_ContentExtract (_clientNum);
        if (PEResult.isEnd && !PCResult.isEnd && !PEResult.err)
        {
          if (PEResult.result == 13)  sm_pi_state = sm_pi_EndingStage_0;// if char is CR
          else
          {
            sm_pi_state = sm_pi_error;
            Serial.println("PI State Machine error: sm_pi_fullEnding_1.");
          }
        }
        else if (PEResult.err || PCResult.isEnd)
        {
          sm_pi_state = sm_pi_error;
          Serial.println("PI State Machine error: sm_pi_fullEnding_0.");
        }
      break;

      case sm_pi_EndingStage_0:
        _num = serverClients[_clientNum].read();
        if (_num == '[') sm_pi_state = sm_pi_EndingStage_1;
        else
        {
          sm_pi_state = sm_pi_error;  // uncorrenct format data
          Serial.print("PI State Machine error: sm_pi_EndingStage_0");
        }
      break;
      
      case sm_pi_EndingStage_1:
        ProcessIncoming_ContentExtract (_clientNum);
        if (PEResult.isEnd && !PCResult.isEnd && !PEResult.err)
        {
          if (PEResult.result == 10) // if char is LF
          {
            if (leftMotorTemp != -999)
            {
              leftMotorCommand = leftMotorTemp;
              leftMotorUpdateFlag = true;
            }
            else  leftMotorUpdateFlag = false;
            
            if (rightMotorTemp != -999)
            {
              rightMotorCommand = rightMotorTemp;
              rightMotorUpdateFlag  = true;
            }
            else  rightMotorUpdateFlag  = false;
            
            if (midMotorTemp != -999)
            {
              midMotorCommand = midMotorTemp;
              midMotorUpdateFlag = true;
            }
            else  midMotorUpdateFlag  = false;
            
            if (speedTemp !=  -999) speedCommand = speedTemp;
            /*
            Serial.println("Good, get end");
            Serial.print("leftMotorCommand:  ");
            Serial.println(leftMotorTemp);
            Serial.print("rightMotorTemp:  ");
            Serial.println(rightMotorTemp);
            Serial.print("midMotorTemp:  ");
            Serial.println(midMotorTemp);
            Serial.print("speedTemp:  ");
            Serial.println(speedTemp);
            */
            
            CleanCommandTemp (); // reset motor command temp
            sm_pi_state = sm_pi_clearBeforeGoBack;  // prepare go back to beginning
          }
          else
          {
            sm_pi_state = sm_pi_error;
            Serial.println("PI State Machine error: sm_pi_EndingStage_1_1.");
          }
        }
        else if (PEResult.err || PCResult.isEnd)
        {
          sm_pi_state = sm_pi_error;
          Serial.println("PI State Machine error: sm_pi_EndingStage_1_0.");
        }
      break;
      
      case sm_pi_leftMotor: // take out left motor command
        ProcessIncoming_ContentExtract (_clientNum);
        if (PCResult.isEnd && !PEResult.isEnd && !PCResult.err) // when tacking content value should not receive ending mark
        {
          leftMotorTemp = Convert_String_to_Int (PCResult.result); // convert string to int
          sm_pi_state = sm_pi_clearBeforeGoBack;
        }
        else if (PCResult.err || PEResult.isEnd)
        {
          sm_pi_state = sm_pi_error;
          Serial.println("PI State Machine error: sm_pi_leftMotor.");
        }
      break;
      
      case sm_pi_rightMotor: // take out right motor command
        ProcessIncoming_ContentExtract (_clientNum);
        if (PCResult.isEnd && !PEResult.isEnd && !PCResult.err)
        {
          rightMotorTemp = Convert_String_to_Int (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
        }
        else if (PCResult.err || PEResult.isEnd)
        {
          sm_pi_state = sm_pi_error;
          Serial.println("PI State Machine error: sm_pi_rightMotor.");
        }
      break;
      
      case sm_pi_midMotor: // take out mid motor command
        ProcessIncoming_ContentExtract (_clientNum);
        if (PCResult.isEnd && !PEResult.isEnd && !PCResult.err)
        {
          midMotorTemp = Convert_String_to_Int (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
        }
        else if (PCResult.err || PEResult.isEnd)
        {
          sm_pi_state = sm_pi_error;
          Serial.println("PI State Machine error: sm_pi_midMotor.");
        }
      break;
      
      case sm_pi_speed: // take out speed command
        ProcessIncoming_ContentExtract (_clientNum);
        if (PCResult.isEnd && !PEResult.isEnd && !PCResult.err)
        {
          speedTemp = Convert_String_to_Int (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
        }
        else if (PCResult.err || PEResult.isEnd)
        {
          sm_pi_state = sm_pi_error;
          Serial.println("PI State Machine error: sm_pi_speed.");
        }
      break;
    }
  }
  if (PCResult.err || PEResult.err)
  {
    CleanPISturct ();  //reset structure
    Serial.println("PI outside error: structure err..");
  }
}

void ProcessIncoming_ContentExtract (int _clientNum) // extract data from incoming message before break mark
{
  ProcessContentResult* _cResult = &PCResult;
  ProcessEndingResult* _eResult = &PEResult;
  
  char _num;
  while (serverClients[_clientNum].available()) // loop until package finish
  {
    _num = serverClients[_clientNum].read();
    if (_num == ')' ||  _num == ';')
    {
      _cResult->result = String (pi_buff);
      _cResult->isEnd = true;
      CleanPIBuff (); // got reuslt, can reset buffer
      return;
    }
    else if (_num == ']')
    {
      _eResult->result = pi_buff[pi_bufferCount - 1];
      _eResult->isEnd = true;
      CleanPIBuff (); // got reuslt, can reset buffer
      return;
    }
    pi_buff[pi_bufferCount] = _num;
    pi_bufferCount ++;
    if (pi_bufferCount > 7)
    {
      _cResult->err = true; // if content get more then 7 letter
      CleanPIBuff ();
      return;
    }
  }
  /*
  _cResult->err = true; // content result and this command should not be execute
  _eResult->err = true; // content result and this command should not be execute
  CleanPIBuff ();
  Serial.println("PI content error: Error occured, content process unexpected failed");
  */
}

void ResetMotorFlag ()
{
  leftMotorUpdateFlag = rightMotorUpdateFlag = midMotorUpdateFlag = false;
}

void CleanCommandTemp ()
{
  leftMotorTemp = rightMotorTemp = midMotorTemp = speedTemp = -999;  // clean temp stroge valuable
}

void CleanPISturct () 
{
  PCResult = ProcessContentResult ();  //reset structure
  PEResult = ProcessEndingResult ();  //reset structure
}

void CleanPIBuff ()
{
  memset(pi_buff, 0, sizeof(pi_buff));  // zero the buffer
  pi_bufferCount = 0;
}

int Convert_String_to_Int (String _str)
{
  char _takeNum [_str.length() + 1]; // don't know why toCharArray will not return full lenght number if array length exactly same as string length. May be first position of array has been occupied by other usage, so add 1 more position.
  _str.toCharArray (_takeNum, sizeof (_takeNum)); // string to array
  
  return atoi(_takeNum);  // array to int
}

void MotorCommandAutoReset ()
{
  int resetTime = 500;

  // left motor
  if (!leftMotorUpdateFlag && leftMotorTimer >= 0) // only execute if there is no new command come and put this judgement early make sure detect before reset flag
  {
    if (TimeCounter (leftMotorTimer, resetTime))  // reset movement after time
    {
      leftMotorCommand = leftMotorTimer = 0;
      //Serial.println(leftMotorCommand * speedCommand);
    }
  }
  if (leftMotorUpdateFlag)
  {
    leftMotorTimer = millis(); // start countting time
    leftMotorUpdateFlag = false;
  }

  // right motor
  if (!rightMotorUpdateFlag && rightMotorTimer >= 0)
  {
    if (TimeCounter (rightMotorTimer, resetTime))  // reset movement after time
    {
      rightMotorCommand = rightMotorTimer = 0;
      //Serial.println(rightMotorCommand * speedCommand);
    }
  }
  if (rightMotorUpdateFlag)
  {
    rightMotorTimer = millis(); // start countting time
    rightMotorUpdateFlag = false;
  }

  // mid motor
  if (!midMotorUpdateFlag && midMotorTimer >= 0)
  {
    if (TimeCounter (midMotorTimer, resetTime))  // reset movement after time
    {
      midMotorCommand = midMotorTimer = 0;
      //Serial.println(midMotorCommand * speedCommand);
    }
  }
  if (midMotorUpdateFlag)
  {
    midMotorTimer = millis(); // start countting time
    midMotorUpdateFlag = false;
  }
}

void MotorControl ()
{
  // TopNum(150) * Command * Factor(11/15) + bottomNum(40)
  // TopSpeed = 150 * 1 * 11/15 + 40 = 150
  // BottomSpeed = 150 * 0 * 11/15 + 40 = 40
  if (leftMotorCommand >= 0)  analogWrite(lMotorPin, (150.0 * (float(leftMotorCommand * speedCommand)/10000.0)) * (11.0/15.0) + 40.0); // pin 0, PWM Speed Control left motor
  else if (leftMotorCommand < 0)  analogWrite(lMotorPin, (150.0 * (float(-leftMotorCommand * speedCommand)/10000.0)) * (11.0/15.0) + 40.0);
  
  if (rightMotorCommand >= 0)  analogWrite(rMotorPin, (150.0 * (float(rightMotorCommand * speedCommand)/10000.0)) * (11.0/15.0) + 40.0); // pin 2, PWM Speed Control right motor
  else if (rightMotorCommand < 0)  analogWrite(lMotorPin, (150.0 * (float(-rightMotorCommand * speedCommand)/10000.0)) * (11.0/15.0) + 40.0);

  if (midMotorCommand >= 0)  analogWrite(mMotorPin, (150.0 * (float(midMotorCommand * speedCommand)/10000.0)) * (11.0/15.0) + 40.0); // pin 4, PWM Speed Control mid motor
  else if (midMotorCommand < 0)  analogWrite(lMotorPin, (150.0 * (float(-midMotorCommand * speedCommand)/10000.0)) * (11.0/15.0) + 40.0);

  if (leftMotorReverseFlag)  digitalWrite(lMotorReversePin,HIGH);
  else  digitalWrite(lMotorReversePin,LOW);
}

void LeftMontorControl (float _num)
{
  // TopNum(150) * Command * Factor(11/15) + bottomNum(40)
  // TopSpeed = 150 * 1 * 11/15 + 40 = 150
  // BottomSpeed = 150 * 0 * 11/15 + 40 = 40
  analogWrite(lMotorPin, (150.0 * (float(_num * speedCommand)/10000.0)) * (11.0/15.0) + 40.0); // pin 0, PWM Speed Control left motor
}

void DebugAndTest ()
{
  
}

// use to calculate how long past after previous time
// _PT: previous time, RT: reset time
bool TimeCounter (unsigned long _PT, int _RT)
{
  unsigned long _rollover = 0;
  if (millis() - _PT < 0) // if millis has rollover
  {
    _rollover = _PT;
  }
  
  if ((unsigned long)((_rollover + millis()) - _PT) >= _RT)
  {
    return true;
  }
  else
  {
    return false;
  }
}


