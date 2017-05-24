#include <ESP8266WiFi.h>
#define HOSTNAME "ControlBoard" // useless
#define MAX_SRV_CLIENTS 1
#define MAX_Buff_Length 7

typedef struct
{
  String result;
  bool isEnd;
  bool headerEnd;
  bool contentEnd;
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

IPAddress IP(10,0,0,50); 
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
uint8_t sm_pi_state = -10; //  sate machine for process incomming data
const uint8_t sm_pi_debug = -20;
const uint8_t sm_pi_error = -15;
const uint8_t sm_pi_clearBeforeGoBack = -11; // state machine processing incoming data, analyse the header
const uint8_t sm_pi_begin = -10;
const uint8_t sm_pi_headerExtract = 0;
const uint8_t sm_pi_contantExtract = 1;
const uint8_t sm_pi_headerAnalyse = 10; // state machine processing incoming data, analyse the header
const uint8_t sm_pi_fullEnding = 20;
const uint8_t sm_pi_EndingStage_0 = 21;
const uint8_t sm_pi_EndingStage_1 = 22;
const uint8_t sm_pi_leftMotor= 30;
const uint8_t sm_pi_rightMotor = 40;
const uint8_t sm_pi_midMotor = 50;
const uint8_t sm_pi_speed = 60;
const uint8_t sm_pi_servo_0 = 70;
const uint8_t sm_pi_light_0 = 80;
const uint8_t sm_pi_light_1 = 90;
const uint8_t sm_pi_resetMode = 100;
const uint8_t sm_pi_resetL = 101;
const uint8_t sm_pi_resetR = 102;
const uint8_t sm_pi_resetM = 103;

uint8_t sm_workingMode = -10;
const uint8_t sm_start = -10;
const uint8_t sm_normal = 0;
const uint8_t sm_reset = 10;

char pi_buff[MAX_Buff_Length];
uint8_t pi_bufferCount = 0;

float speedCommand = 0;  // can not use uint8_t, because will get incorrect number when trying to return negative uint8_t value from function
float leftMotorCommand = 0;
float rightMotorCommand = 0;
float midMotorCommand = 0;
float servoMotorCommand_0 = 0;
bool lightCommand_0 = false;
bool lightCommand_1 = false;
float debugCommand = 0; // only use to debug
bool resetMode = false; // startting motor reset mode
bool resetLeft = false; // reset left motor
bool resetRight = false; // reset right motor
bool resetMid = false; // reset middle


uint8_t headerTemp;
float speedTemp = -999;
float leftMotorTemp = -999;
float rightMotorTemp = -999;
float midMotorTemp = -999;
float servoMotorTemp_0 = -999;
int lightTemp_0 = -999;
int lightTemp_1 = -999;
float debugTemp = -999;
int rmodeTemp = -999;
int rlTemp = -999;
int rrTemp = -999;
int rmTemp = -999;

bool leftMotorUpdateFlag = false;
bool rightMotorUpdateFlag = false;
bool midMotorUpdateFlag = false;
bool lightState_0 = false;
bool lightState_1 = false;

unsigned long leftMotorTimer = 0;
unsigned long rightMotorTimer = 0;
unsigned long midMotorTimer = 0;
unsigned long ledOffTimer = 0;
unsigned long ledOnTimer = 0;
unsigned long starterTimer = 0;

unsigned long LMRTimer = 0; // left motor reverse timer,use to delay after reverse command come
unsigned long RMRTimer = 0; // right motor reverse timer
unsigned long MMRTimer = 0; // middle motor reverse timer

/*
uint8_t lMotorPin = 0;  // pin 0, ESPDuino ESP-13
uint8_t rMotorPin = 2;  // pin 2
uint8_t mMotorPin = 4;
*/
///*
uint8_t lMotorPin = 5;  // pin 1, NodeMCU ESP-12E
uint8_t rMotorPin = 4;  // pin 2
uint8_t mMotorPin = 0;  // pin 3
uint8_t camMotor_0_Pin = 2;  // pin 4
uint8_t light_0_Pin = 14;  // pin 5
uint8_t light_1_Pin = 12;  // pin 6
//uint8_t mMotorReversePin = 13;  // pin 7
uint8_t debugSpeedPin = 15;  // pin 8
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
  
  analogWriteFreq(50);  // set PWM frequence as 50hz
  pinMode(lMotorPin, OUTPUT); // initialize pin out is necessary, althought don't initialize still can use pwm function but that require pwm out put low and high to work
  pinMode(rMotorPin, OUTPUT); // if no initialization and directly use pwm to control will not get correct value
  pinMode(mMotorPin, OUTPUT);
  
  pinMode(camMotor_0_Pin, OUTPUT);
  
  pinMode(light_0_Pin, OUTPUT);
  pinMode(light_1_Pin, OUTPUT);
  //pinMode(mMotorReversePin, OUTPUT);
  pinMode(debugSpeedPin, OUTPUT);

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
      Socket_Communication (i); // monitor cache for incoming data and process it 
  }

  switch (sm_workingMode)
  {
    case sm_start:
      if (starterTimer == 0) starterTimer = millis ();
      if (TimeCounter (ledOffTimer, 1000)) sm_workingMode = sm_normal;
      else MotorCommandAutoReset ();
    break;
    
    case sm_normal:
      if (resetMode == true) sm_workingMode = sm_reset;
      else
      {
        resetLeft = resetRight = resetMid
        = false;
      }
      
      MotorCommandAutoReset (); // auto reset motor command if motor not receiving command any more
  
      MotorControl ();  // move motor

      ServoMotorControl (); // move cam

      LightControl ();

      DebugAndTest ();
    break;
    
    case sm_reset:
      if (resetMode == false) sm_workingMode = sm_normal;
      
      ResetMotor ();
    break;
  }
}

void ResetMotor () // control servo motor to move cam
{
    
    if (resetLeft == true) analogWrite(lMotorPin, 103); // pin 0, PWM Speed Control left motor
    else analogWrite(lMotorPin, 77.3388);
    
    if (resetRight == true) analogWrite(rMotorPin, 103); // pin 1, PWM Speed Control right motor
    else analogWrite(rMotorPin, 77.3388);
    
    if (resetMid == true) analogWrite(mMotorPin, 103); // pin 2, PWM Speed Control mid motor
    else analogWrite(mMotorPin, 77.3388);
}

void ServoMotorControl () // control servo motor to move cam
{
  analogWrite(camMotor_0_Pin, (25.575 * (servoMotorCommand_0 / 100.0) + 76.725)); // pin 4
}

void LightControl ()  // lighting led control
{
  if (lightCommand_0 != lightState_0) lightState_0 = lightCommand_0;
  if (lightState_0) digitalWrite(light_0_Pin,HIGH);
  else digitalWrite(light_0_Pin,LOW);
  
  if (lightCommand_1 != lightState_1) lightState_1 = lightCommand_1;
  if (lightState_1) digitalWrite(light_1_Pin,HIGH);
  else digitalWrite(light_1_Pin,LOW);
}

void LEDManager ()  // led onboard instructions
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
        if (_num == '(')  sm_pi_state = sm_pi_headerExtract;
        else if (_num == '[') sm_pi_state = sm_pi_fullEnding;
        else
        {
          sm_pi_state = sm_pi_error;  // uncorrenct format data
          Serial.println("PI State Machine error: sm_pi_state");
        }
        //Serial.print("state change: "); // keep this code for future debug
        //Serial.println(sm_pi_state);
      break;

      case sm_pi_headerExtract:
        if (PCResult.isEnd && PCResult.headerEnd && !PCResult.contentEnd && !PEResult.isEnd && !PCResult.err) // when tacking header should not receive ending mark
        {
          String _header = PCResult.result;
          CleanPISturct ();  //reset structure
          sm_pi_state = sm_pi_contantExtract;

          if (_header == "L") headerTemp = sm_pi_leftMotor;
          else if (_header == "R") headerTemp = sm_pi_rightMotor;
          else if (_header == "M") headerTemp = sm_pi_midMotor;
          else if (_header == "SP") headerTemp = sm_pi_speed;
          else if (_header == "SE") headerTemp = sm_pi_servo_0;
          else if (_header == "L0") headerTemp = sm_pi_light_0;
          else if (_header == "L1") headerTemp = sm_pi_light_1;
          else if (_header == "RE") headerTemp = sm_pi_resetMode;
          else if (_header == "Rl") headerTemp = sm_pi_resetL;
          else if (_header == "Rr") headerTemp = sm_pi_resetR;
          else if (_header == "Rm") headerTemp = sm_pi_resetM;
          else
          {
            sm_pi_state = sm_pi_error;  // uncorrenct format data
            Serial.println("PI State Machine error: sm_pi_headerExtract - 03.");
          }
        }
        else if (PCResult.contentEnd)
        {
          sm_pi_state = sm_pi_error;  // uncorrenct format data
          Serial.println("PI State Machine error: sm_pi_headerExtract - 02.");
        }
        else if (PEResult.isEnd)
        {
          sm_pi_state = sm_pi_error;  // uncorrenct format data
          Serial.println("PI State Machine error: sm_pi_headerExtract - 01.");
        }
        else if (PCResult.err)
        {
          sm_pi_state = sm_pi_error;  // uncorrenct format data
          Serial.println("PI State Machine error: sm_pi_headerExtract - 00.");
        }
        else ProcessIncoming_ContentExtract (_clientNum); // extract header from incoming message
      break;
      
      case sm_pi_contantExtract:
      
        if (PCResult.isEnd && PCResult.contentEnd && !PCResult.headerEnd && !PEResult.isEnd && !PCResult.err)
        {
          sm_pi_state = headerTemp; // go save command according to header
        }
        else if (PCResult.headerEnd)
        {
          sm_pi_state = sm_pi_error;  // uncorrenct format data
          Serial.println("PI State Machine error: sm_pi_contantExtract - 02.");
        }
        else if (PEResult.isEnd)
        {
          sm_pi_state = sm_pi_error;  // uncorrenct format data
          Serial.println("PI State Machine error: sm_pi_contantExtract - 01.");
        }
        else if (PCResult.err)
        {
          sm_pi_state = sm_pi_error;  // uncorrenct format data
          Serial.println("PI State Machine error: sm_pi_contantExtract - 00.");
        }
        else ProcessIncoming_ContentExtract (_clientNum); // extract command from incoming message
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
        if (PEResult.isEnd && !PCResult.isEnd && !PEResult.err)
        {
          if (PEResult.result == 13)
          {
            CleanPISturct ();  //reset structure
            sm_pi_state = sm_pi_EndingStage_0;// if char is CR
          }
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
        else ProcessIncoming_ContentExtract (_clientNum);
      break;

      case sm_pi_EndingStage_0:
        _num = serverClients[_clientNum].read();
        if (_num == '[') sm_pi_state = sm_pi_EndingStage_1;
        else
        {
          sm_pi_state = sm_pi_error;  // uncorrenct format data
          Serial.println("PI State Machine error: sm_pi_EndingStage_0");
        }
      break;
      
      case sm_pi_EndingStage_1:
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
            if (servoMotorTemp_0 != -999) servoMotorCommand_0 = servoMotorTemp_0;

            // light commands
            if (lightTemp_0 > 0) lightCommand_0 = true;
            else if (lightTemp_0 == 0) lightCommand_0 = false;
            if (lightTemp_1 > 0) lightCommand_1 = true;
            else if (lightTemp_1 == 0) lightCommand_1 = false;

            //  reset mode commands
            if (rmodeTemp > 0) resetMode = true;
            else if (rmodeTemp == 0) resetMode = false;
            if (rlTemp > 0) resetLeft = true;
            else if (rlTemp == 0) resetLeft = false;
            if (rrTemp > 0) resetRight = true;
            else if (rrTemp == 0) resetRight = false;
            if (rmTemp > 0) resetMid = true;
            else if (rmTemp == 0) resetMid = false;
            
            if (debugTemp != -999) debugCommand = debugTemp;
            
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
        else ProcessIncoming_ContentExtract (_clientNum);
      break;
      
      case sm_pi_leftMotor: // take out left motor command
          leftMotorTemp = Convert_String_to_Float (PCResult.result); // convert string to int
          sm_pi_state = sm_pi_clearBeforeGoBack;
      break;
      
      case sm_pi_rightMotor: // take out right motor command
          rightMotorTemp = Convert_String_to_Float (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
      break;
      
      case sm_pi_midMotor: // take out mid motor command
          midMotorTemp = Convert_String_to_Float (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
      break;
      
      case sm_pi_speed: // take out speed command
          speedTemp = Convert_String_to_Float (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
      break;

      case sm_pi_servo_0: // take out servo_0 command
          servoMotorTemp_0 = Convert_String_to_Float (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
      break;

      case sm_pi_light_0: // take out light_0 command
          lightTemp_0 = Convert_String_to_Int (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
      break;
      
      case sm_pi_light_1: // take out light_1 command
          lightTemp_1 = Convert_String_to_Int (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
      break;

      case sm_pi_debug: // take out debug command
          debugTemp = Convert_String_to_Float (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
      break;

      case sm_pi_resetMode: // reset mode
          rmodeTemp = Convert_String_to_Int (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
      break;
      
      case sm_pi_resetL: // reset left motor
          rlTemp = Convert_String_to_Int (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
      break;
      
      case sm_pi_resetR: // reset right motor
          rrTemp = Convert_String_to_Int (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
      break;
      
      case sm_pi_resetM: // reset mid motor
          rmTemp = Convert_String_to_Int (PCResult.result);
          sm_pi_state = sm_pi_clearBeforeGoBack;
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
    if (_num == ')')
    {
      _cResult->result = String (pi_buff);
      _cResult->isEnd = true;
      _cResult->headerEnd = true;
      CleanPIBuff (); // got reuslt, can reset buffer
      return;
    }
    else if (_num == ';')
    {
      _cResult->result = String (pi_buff);
      _cResult->isEnd = true;
      _cResult->contentEnd = true;
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
    else
    {
      pi_buff[pi_bufferCount] = _num;
      pi_bufferCount ++;
    }
    
    if (pi_bufferCount > MAX_Buff_Length)
    {
      _cResult->err = true; // if content get more then MAX_Buff_Length
      _eResult->err = true;
      CleanPIBuff ();
      return;
    }
  }
}

void ResetMotorFlag ()
{
  leftMotorUpdateFlag = rightMotorUpdateFlag = midMotorUpdateFlag = false;
}

void CleanCommandTemp ()
{
  leftMotorTemp = rightMotorTemp = midMotorTemp = speedTemp
  = servoMotorTemp_0 = debugTemp
  = lightTemp_0 = lightTemp_1
  = rmodeTemp = rlTemp = rrTemp = rmTemp
  = -999;  // clean temp stroge valuable
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

float Convert_String_to_Float (String _str)
{
  char _takeNum [_str.length() + 1]; // don't know why toCharArray will not return full lenght number if array length exactly same as string length. May be first position of array has been occupied by other usage, so add 1 more position.
  _str.toCharArray (_takeNum, sizeof (_takeNum)); // string to array
  
  return atof(_takeNum); // array to float
}

int Convert_String_to_Int (String _str)
{
  char _takeNum [_str.length() + 1];
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
  // TopNum(90) * Command + bottomNum(40)
  // TopSpeed = 27 * 1 + 76 = 103
  // BottomSpeed = 27 * -1 + 76 = 49
  analogWrite(lMotorPin, (25.6612 * (float(leftMotorCommand * speedCommand)/10000.0)) + 77.3388); // pin 0, PWM Speed Control left motor
  
  analogWrite(rMotorPin, (25.6612 * (float(rightMotorCommand * speedCommand)/10000.0)) + 77.3388); // pin 2, PWM Speed Control right motor

  analogWrite(mMotorPin, (25.6612 * (float(midMotorCommand * speedCommand)/10000.0)) + 77.3388); // pin 4, PWM Speed Control mid motor
}

void LeftMontorControl (float _num)
{
  // TopNum(150) * Command * Factor(11/15) + bottomNum(40)
  // TopSpeed = 150 * 1 * 11/15 + 40 = 150
  // BottomSpeed = 150 * 0 * 11/15 + 40 = 40
  analogWrite(lMotorPin, (150.0 * (float(_num * speedCommand)/10000.0)) * (11.0/15.0) + 40.0); // pin 0, PWM Speed Control left motor
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

void DebugAndTest ()
{
  if (debugCommand != 0)
  {
    analogWrite(debugSpeedPin, (1023.0 * (float(debugCommand * speedCommand)/10000.0)));
  }
  //Serial.print("leftMotorCommand:  ");
  //Serial.println(leftMotorCommand);
  //Serial.print("rightMotorCommand:  ");
  //Serial.println(rightMotorCommand);
  //Serial.print("speedCommand:  ");
  //Serial.println(speedCommand);
  //Serial.print("servoMotorCommand_0:  ");
  //Serial.println(servoMotorCommand_0);
  //Serial.print("debugCommand:  ");
  //Serial.println(debugCommand);
  
  //Serial.println("PI State Machine error: sm_pi_leftMotor.");
}

