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

uint8_t lMotorPin = 0;
uint8_t rMotorPin = 2;
uint8_t mMotorPin = 4;

void setup() {
  //pinMode(LED_BUILTIN, OUTPUT); // initiallize LED
  
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
  /*
  Serial.println("121212");
  pinMode(1, OUTPUT);
  pinMode(13, OUTPUT); 
  Serial.println("565656");
  /*
  pinMode(0, OUTPUT); 
  pinMode(2, OUTPUT); 
  pinMode(4, OUTPUT); 
  pinMode(5, OUTPUT); 
  */
  analogWriteFreq(50);
  pinMode(lMotorPin, OUTPUT); // initialize pin out is necessary, althought don't initialize still can use pwm function but that require pwm out put low and high to work
  pinMode(rMotorPin, OUTPUT); // if no initialization and directly use pwm to control will not get correct value
  pinMode(mMotorPin, OUTPUT);
}

void loop() {

  //digitalWrite(LED_BUILTIN, LOW); // when running, keep the LED on
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
  MotorCommandAutoReset ();
  MotorControl ();

  //DebugAndTest ();
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
    if (pi_bufferCount > 5)
    {
      _cResult->err = true; // if content get more then 5 letter
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
  //Serial.print("LLLLLLLL:      ");
  //Serial.println(1023 * (float(leftMotorCommand * speedCommand)/10000));
  analogWrite(lMotorPin, 1023 * (float(leftMotorCommand * speedCommand)/10000)); // pin 0, PWM Speed Control left motor

  //Serial.print("RRRRRRRR:      ");
  //Serial.println(1023 * (float(rightMotorCommand * speedCommand)/10000));
  analogWrite(rMotorPin, 1023 * (float(rightMotorCommand * speedCommand)/10000)); // pin 2, PWM Speed Control right motor

  analogWrite(mMotorPin, 1023 * (float(midMotorCommand * speedCommand)/10000)); // pin 4, PWM Speed Control mid motor
}

void DebugAndTest ()
{
  //analogWriteFreq(50);
  //analogWrite(0, 1023 * (float(speedCommand)/100)); // pin 0, PWM Speed Control
  //Serial.println(1023 * (float(speedCommand)/100));
  //Serial.println(speedCommand);
  //Serial.println("1");
  //pwm.setPWM(servoNum, 0, 102);
  //delay(5000);
  //Serial.println("2");
  //pwm.setPWM(servoNum, 0, 512);
  //delay(5000);
  
  //digitalWrite(dir1,LOW);  // pin 1
  //digitalWrite(dir2,HIGH);  // pin 13
  //analogWrite(pwm1, 255); // pin 3, PWM Speed Control
  //analogWrite(pwm2, 255); // pin 12 PWM Speed Control
  /*
  Serial.println("1");

  digitalWrite(1,HIGH);  // pin 1
  analogWrite(3, 1023); // pin 3, PWM Speed Control
  digitalWrite(13,HIGH);  // pin 13
  analogWrite(12, 1023); // pin 12 PWM Speed Control
  delay(5000);
  Serial.println("2");
  digitalWrite(1,HIGH);  // pin 1
  analogWrite(3, 0); // pin 3, PWM Speed Control
  digitalWrite(13,LOW);  // pin 13
  analogWrite(12, 0); // pin 12 PWM Speed Control
  delay(5000);

  /*
  Serial.println("1");
  analogWrite(0, 0);
  analogWrite(2, 255);
  analogWrite(4, 768);
  analogWrite(5, 1023);
  delay(5000);
  Serial.println("2");
  analogWrite(0, 1023);
  analogWrite(2, 768);
  analogWrite(4, 255);
  analogWrite(5, 0);
  delay(5000);
  */
  /*
  Serial.println("1");
  digitalWrite(0, HIGH);
  digitalWrite(2, LOW);
  digitalWrite(4, HIGH);
  digitalWrite(5, LOW);
  delay(5000);
  Serial.println("2");
  digitalWrite(0, LOW);
  digitalWrite(2, HIGH);
  digitalWrite(4, LOW);
  digitalWrite(5, HIGH);
  delay(5000);
  */
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


