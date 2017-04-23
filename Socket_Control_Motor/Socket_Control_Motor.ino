#include <ESP8266WiFi.h>
#include <PWM.h>
#define HOSTNAME "ControlBoard" // useless
#define MAX_SRV_CLIENTS 1
#define MAX_Buff_Length 5

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
const uint8_t sm_pi_headerAnalyse = 0; // state machine processing incoming data, analyse the header
const uint8_t sm_pi_error = 5;
//const uint8_t sm_pi_contentExtract = 10; // may be is wrong code, no need for contentEnding
const uint8_t sm_pi_fullEnding = 20;
const uint8_t sm_pi_leftMotor= 30;
const uint8_t sm_pi_rightMotor = 40;
const uint8_t sm_pi_midMotor = 50;
const uint8_t sm_pi_speed = 60;

int speedCommand = 30;  // can not use uint8_t, because will get incorrect number when trying to return negative uint8_t value from function
int leftMotorCommand = 0;
int rightMotorCommand = 0;
int midMotorCommand = 0;

// motor control pin assign
const uint8_t pwm1 = 3;
const uint8_t dir1 = 1;
const uint8_t pwm2 = 12;
const uint8_t dir2 = 13;

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
}

void loop() {

  //digitalWrite(LED_BUILTIN, LOW); // when running, keep the LED on
  uint8_t i;  // "uint8_t" its shorthand for: a type of unsigned integer of length 8 bits
  
  // if client is connected in
  if (server.hasClient())
  {
    // Looking for dissconnected client connection and make it availabe for new connection
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
    serverClient.stop();
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

  if (leftMotorCommand != 0)
  {
    Serial.println(leftMotorCommand);
    leftMotorCommand = 0;
  }
  if (rightMotorCommand != 0)
  {
    Serial.println(rightMotorCommand);
    rightMotorCommand = 0;
  }
  if (midMotorCommand != 0)
  {
    Serial.println(midMotorCommand);
    midMotorCommand = 0;
  }
  if (speedCommand != 30)
  {
    Serial.println(speedCommand);
    speedCommand = 30;
  }

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

void Socket_Communication (int _clientNum)
{
  bool _newMessageComing = false;
  bool _newMessageCompleted = false;
  int _oldSpeedCommand = -99;
  uint8_t _pi_state = sm_pi_headerAnalyse; //  sate machine process incomming data
  while(serverClients[_clientNum].available()) // loop when there are data coming
  {
    _newMessageComing = true;
    char _num;
    switch (_pi_state)
    {
      case sm_pi_headerAnalyse: // analyse the header to understander what command comes
      _num = serverClients[_clientNum].read();
      if (_num == '(')
      {
        String _header = ProcessIncoming_contentExtract (_clientNum);  // extract header from incoming message

        if (_header == "L")  _pi_state = sm_pi_leftMotor;
        else if (_header == "R")  _pi_state = sm_pi_rightMotor;
        else if (_header == "M")  _pi_state = sm_pi_midMotor;
        else if (_header == "SP")  _pi_state = sm_pi_speed;
        else  _pi_state = sm_pi_error;  // uncorrenct format data
      }
      else if (_num == '[') _pi_state = sm_pi_fullEnding;
      else _pi_state = sm_pi_error;  // uncorrenct format data
      //Serial.print("state change: "); keep this code for future debug
      //Serial.println(_pi_state);
      break;
      case sm_pi_error: // if is not correct format or other error
        serverClients[_clientNum].flush();  // flush cache
        Serial.println("PI State Machine error: Error occured, Format not correct.");
      break;
      case sm_pi_fullEnding:
        _num = serverClients[_clientNum].read();
        if (_num == 13) // if get CR
        {
          _num = serverClients[_clientNum].read();
          if (_num == ']')
          {
            _num = serverClients[_clientNum].read();
            if (_num == '[')
            {
              _num = serverClients[_clientNum].read();
              if (_num == 10) // if char is LF
              {
                _num = serverClients[_clientNum].read();
                if (_num == ']')
                {
                  if (!serverClients[_clientNum].available()) _newMessageCompleted = true;  // format fit, commands accpeted
                  else  Serial.println("PI State Machine error: there are data followed after finish mark.");
                }
                else _pi_state = sm_pi_error;
              }
              else _pi_state = sm_pi_error;
            }
            else _pi_state = sm_pi_error;
          }
          else _pi_state = sm_pi_error; // the data is not fit transfer format
        }
        else _pi_state = sm_pi_error;  // uncorrenct format data 
      break;
      case sm_pi_leftMotor: // take out left motor command
        leftMotorCommand = Convert_String_to_Int (ProcessIncoming_contentExtract (_clientNum)); // convert string to int
        _pi_state = sm_pi_headerAnalyse;
      break;
      case sm_pi_rightMotor: // take out right motor command
        rightMotorCommand = Convert_String_to_Int (ProcessIncoming_contentExtract (_clientNum));
        _pi_state = sm_pi_headerAnalyse;
      break;
      case sm_pi_midMotor: // take out mid motor command
        midMotorCommand = Convert_String_to_Int (ProcessIncoming_contentExtract (_clientNum));
        _pi_state = sm_pi_headerAnalyse;
      break;
      case sm_pi_speed: // take out speed command
        _oldSpeedCommand = speedCommand;
        speedCommand = Convert_String_to_Int (ProcessIncoming_contentExtract (_clientNum));
        _pi_state = sm_pi_headerAnalyse;
      break;
    }
  }
  if (_newMessageComing && !_newMessageCompleted) // if there is income message but the format is not correct then clear all containers
  {
    leftMotorCommand = rightMotorCommand = midMotorCommand = 0; // clear command container
    if (_oldSpeedCommand != -99) speedCommand = _oldSpeedCommand; // regain old speed if speed command has been changed
    Serial.println("Incoming Message Error: Process not finish.");
  }
}

String ProcessIncoming_contentExtract (int _clientNum) // extract data from incoming message before break mark
{
  char _buff[MAX_Buff_Length];
  memset(_buff, 0, sizeof(_buff));  // zero the buffer
  uint8_t _bufferCount = 0;
  char _num = serverClients[_clientNum].read();
  while (_num != ')' && _num != ';')
  {
    _buff[_bufferCount] = _num;
    _bufferCount ++;
    if (_bufferCount > 5)  return "Error"; // if content get more then 5 letter
    _num = serverClients[_clientNum].read();
  }
  
  return String (_buff);  // return result
}

int Convert_String_to_Int (String _str)
{
  char _takeNum [_str.length() + 1]; // don't know why toCharArray will not return full lenght number if array length exactly same as string length. May be first position of array has been occupied by other usage, so add 1 more position.
  _str.toCharArray (_takeNum, sizeof (_takeNum)); // string to array
  
  return atoi(_takeNum);  // array to int
}



