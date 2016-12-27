/*  
 *   For a LED lamp with an ESP-01 chip.
 *   Most code by Thomas Friberg
 *   Updated 27/12/2016
 */

// Import ESP8266 libraries
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

//Sensor details
const char* sensor1ID = "RET003"; //Name of sensor
const char* sensor1Type = "2";
const char* sensor1Description = "Reticulation";
const int defaultFade = 15; //Miliseconds between fade intervals - Think about putting this in EEPROM
const int ledPin = 2; //LED pin number

// WiFi parameters
const char* ssid = "TheSubway"; //Enter your WiFi network name here in the quotation marks - Will need to be in EEPROM
const char* password = "vanillamoon576"; //Enter your WiFi pasword here in the quotation marks - Will need to be in EEPROM

//Server details
unsigned int localPort = 5007;  //UDP send port
const char* ipAdd = "192.168.0.100"; //Server address
byte packetBuffer[512]; //buffer for incoming packets

//Sensor variables
int ledPinState = 0; //Default boot state of LEDs and last setPoint of the pin between 0 and 100
int ledSetPoint = 0;
int brightness = 100; //last 'on' setpoint for 0-100 scale brightness
static const unsigned int PWMTable[101] = {0,1,2,3,5,6,7,8,9,10,12,13,14,16,18,20,21,24,26,28,31,33,36,39,42,45,49,52,56,60,64,68,72,77,82,87,92,98,103,109,115,121,128,135,142,149,156,164,172,180,188,197,206,215,225,235,245,255,266,276,288,299,311,323,336,348,361,375,388,402,417,432,447,462,478,494,510,527,544,562,580,598,617,636,655,675,696,716,737,759,781,803,826,849,872,896,921,946,971,997,1023}; //0 to 100 values for brightnes
int fadeSpeed = defaultFade; //Time between fade intervals - 20ms between change in brightness
String data = "";
int timerCount = 0;

WiFiUDP Udp; //Instance to send packets


//--------------------------------------------------------------

void setup()
{
  SetupLines();
}

//--------------------------------------------------------------


void loop()
{
  //Check timer state
  CheckTimer();
  
  data=ParseUdpPacket(); //Code for receiving UDP messages
  if (data!="") {
    ProcessLedMessage(data);//Conditionals for switching based on LED signal
  }
  
  FadeLEDs(); //Fading script

  if (WiFi.status() != WL_CONNECTED) {
    ConnectWifi();
  }
}

//--------------------------------------------------------------





void SetupLines() {
  //Set pins and turn off LED
  pinMode(ledPin, OUTPUT); //Set as output
  digitalWrite(ledPin, 0); //Turn off LED while connecting
  
  // Start //Serial port monitoring
  //Serial.begin(115200);
  
  ConnectWifi();
  
  //digitalWrite(ledPin, HIGH); //Turn off LED while connecting
  //delay(50); //A flash of light to confirm that the lamp is ready to take commands
  digitalWrite(ledPin, LOW); //Turn off LED while connecting
  ledSetPoint=0; // input a setpoint for fading as we enter the loop
}

void ConnectWifi() {
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print(".");
  }
  //Serial.println("");
  //Serial.print("WiFi connected with IP: ");
  // Print the IP address
  //Serial.println(WiFi.localIP());

  //Open the UDP monitoring port
  Udp.begin(localPort);
  //Serial.print("Udp server started at port: ");
  //Serial.println(localPort);

  //Register on the network with the server after verifying connect
  delay(2000); //Time clearance to ensure registration
  SendUdpValue("REG",sensor1ID,String(sensor1Description)); //Register LED on server
}


String ParseUdpPacket() {
  int noBytes = Udp.parsePacket();
  String udpData = "";
  if ( noBytes ) {
    //Serial.print("---Packet of ");
    //Serial.print(noBytes);
    //Serial.print(" characters received from ");
    //Serial.print(Udp.remoteIP());
    //Serial.print(":");
    //Serial.println(Udp.remotePort());
    // We've received a packet, read the data from it
    Udp.read(packetBuffer,noBytes); // read the packet into the buffer

    // display the packet contents in HEX
    for (int i=1;i<=noBytes;i++) {
      udpData = udpData + char(packetBuffer[i - 1]);
    } // end for
    //Serial.println("Data reads: " + udpData);
  } // end if
  return udpData;
}

void ProcessLedMessage(String dataIn) {
  String devID = "";
  String message = "";

  devID=dataIn.substring(0,6); //Break down the message in to it's parts
  //Serial.println("DevID reads after processing: " + devID);
  message=dataIn.substring(7);
  //Serial.println("Message reads after processing: " + message);
  
  if (devID==sensor1ID) { //Only do this set of commands if there is a message for an LED device
    
    //Enables slow fading
    if (message.startsWith("fade")) { 
      int messagePos=message.indexOf(" ");
      ////Serial.println("Position of space is " + messagePos);
      String fadeVal=message.substring(4,messagePos);
      ////Serial.println("Fade value is " + fadeVal);
      fadeSpeed=atoi(fadeVal.c_str());
      ////Serial.println("Fade speed set to " + fadeSpeed);
      message=message.substring(messagePos+1); //Cutting 'fade' from the message
      //Serial.println("Custom fade increment speed of " + fadeVal + " miliseconds trigged");
      //Serial.println("Message trimmed to : " + message);
    }
    else {
      fadeSpeed=defaultFade;
    }

    //Enables timed commands
    if (message.startsWith("timer")) { 
      int messagePos=message.indexOf(" ");
      ////Serial.println("Position of space is " + messagePos);
      String timerVal=message.substring(5,messagePos);
      ////Serial.println("Fade value is " + fadeVal);
      timerCount=atoi(timerVal.c_str());
      ////Serial.println("Fade speed set to " + fadeSpeed);
      message=message.substring(messagePos+1); //Cutting 'timer' from the message
      //Serial.println("Custom timer of " + timerVal + " seconds set");
      //Serial.println("Message trimmed to: " + message);
    }
    else {
      timerCount=0;
    }

    //Enables instant toggling
    if (((message=="instant toggle")||(message=="instant on")||(message=="instant 100")) && (ledPinState==0)) { //Only turn on if already off
      SendUdpValue("LOG",sensor1ID,String(100));
      ledPinState=100;
      ledSetPoint=100;
      digitalWrite(ledPin, HIGH);
      //Serial.println("---Instant on triggered");
    }
    else if (((message=="instant toggle")  || (message=="instant off") || (message=="instant 0")) && (ledPinState>0)) { //Only turn off if already on
      SendUdpValue("LOG",sensor1ID,String(0));
      ledPinState=0;
      ledSetPoint=0;
      digitalWrite(ledPin, LOW);
      //Serial.println("---Instant off triggered");
    }

    //Enables regular dimming
    if (((message=="toggle")||(message=="on") || ((message=="press") && (ledPinState==ledSetPoint))) && (ledPinState==0) && (ledPinState==0)) { //Only turn on if already off
      SendUdpValue("LOG",sensor1ID,String(brightness));
      ledSetPoint=brightness; // input a setpoint for fading
      //Serial.println("---On triggered");
      //Serial.println("LED state is now has set point of " + String(ledSetPoint));
    }
    else if (((message=="toggle") || (message=="off") || (message=="0") || ((message=="press") && (ledPinState==ledSetPoint))) && (ledPinState>0)) { //Only turn off if already on
      SendUdpValue("LOG",sensor1ID,String(0));
      ledSetPoint=0; // input a setpoint for fading
      //Serial.println("---Off triggered");
      //Serial.println("LED state is now has set point of " + String(ledSetPoint));
    }
    else if (((message=="hold") || (message=="press")) && (ledPinState!=ledSetPoint)) { //For stopping the fade
      brightness=ledPinState;
      SendUdpValue("LOG",sensor1ID,String(brightness));
      ledSetPoint=brightness;
      //Serial.println("LED state is now has set point of " + String(ledSetPoint));
    }
  }
}

void FadeLEDs() {
  if ((millis() % fadeSpeed == 0) && (ledPinState < ledSetPoint)) {
    ledPinState = ledSetPoint;
    analogWrite(ledPin, PWMTable[ledPinState]);
    //Serial.println("LED state is now set to " + String(ledPinState));
    delay(1);
  }
  else if ((millis() % fadeSpeed == 0) && (ledPinState > ledSetPoint)) {
    ledPinState = ledSetPoint;
    analogWrite(ledPin, PWMTable[ledPinState]);
    //Serial.println("LED state is now set to " + String(ledPinState));
    delay(1);
  }
}

void CheckTimer() {
  if(millis() % 1000 == 0) {
    if(timerCount==0) {
      //Do nothing
    }
    else if (timerCount>1) {
      timerCount=timerCount-1;
      //Serial.println("Timer value reduced to " + String(timerCount) + "");
    }
    else {
      timerCount=timerCount-1;
      SendUdpValue("FWD",sensor1ID,"off");
    }
    delay(1);
  }
}

void SendUdpValue(String type, String sensorID, String value) {
  //Print GPIO state in //Serial
  //Serial.print("-Value sent via UDP: ");
  //Serial.println(type + "," + sensorID + "," + value);

  // send a message, to the IP address and port
  Udp.beginPacket(ipAdd,localPort);
  Udp.print(type);
  Udp.write(",");
  Udp.print(sensorID);
  Udp.write(",");
  Udp.print(value); //This is the value to be sent
  Udp.endPacket();
}
