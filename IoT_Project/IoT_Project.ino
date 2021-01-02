// *****************************
// IoT mini project (fingerprint door access)
// Date: 17 august 2020
// By: Mallvin & Kelyn

#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "pitches.h"
#include <Servo.h>
#include <SoftwareSerial.h>
#include <stdlib.h>
#include <ArduinoJson.h>
#define DEBUG true
#define APIKEY "69X1QTSKR4KYV51Q" // kelyn's twitter
Servo myservo;

// String apiKey = "WVVI3FBONACAMYLX"; // mallvin's channel api
String apiKey = "0G4S8S22WUZFMLP2"; // kelyn's channel api
SoftwareSerial ESP01(2, 3); // RX, TX

// notes in the melody(for after when fingerprint is matched)
int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};

// int to store user id of the person whom entered
int id;

// for ultrasonic ranger
long duration, cm = 11;

char* message = "";
char* prevMessage = "";
int door = 0; //locked door by default
int arduino = 1; //on arduino by default

// to keep track of how long the lcd display "scan fingerprint" & fingerprint reading before arduino 'sleeps'(waiting for ultrasonic ranger to sense motion again)
int count = 0;
int count2 = 20;

// to keep track of how many wrong fingerprint is scanned
int wrong = 0;

LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 16 chars and 2 line display

SoftwareSerial mySerial(4, 5);
const int Trig = 6; // Trig connected to pin 13
const int Echo = 7; // Echo connected to pin 12
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// this method is called when fingerprint is matched
void fingerprintSuccess() {
  // display success message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Welcome home!"));
  delay(1000);
  // unlock the door
  myservo.write(0);
  // play the melody
  for (int thisNote = 0; thisNote < 8; thisNote++) {
    int noteDuration = 1000 / noteDurations[thisNote];
    tone(8, melody[thisNote], noteDuration);

    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);

    noTone(8);
  }
  // wait for awhile before locking the door & clear message
  delay(5000);
  myservo.write(90);
  lcd.clear();

  // Send the user id to thingSpeak
  // establish single connection
  Serial.println();
  sendData("AT+RST\r\n", 2000, DEBUG);
  sendData("AT+CWMODE=1\r\n", 2000, DEBUG);
  //sendData("AT+CWJAP=\"Family2.Net\",\"chuleephing7\"\r\n", 4000, DEBUG); // kelyn's home wifi
  sendData("AT+CWJAP=\"Kelyn IX\",\"onetonine\"\r\n", 4000, DEBUG);
  sendData("AT+CIPMUX=0\r\n", 2000, DEBUG);

  // Make TCP connection
  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += "184.106.153.149"; // Thingspeak.com's IP address
  cmd += "\",80\r\n";
  sendData(cmd, 2000, DEBUG);

  // Prepare GET string
  String getStr = "GET /update?api_key=";
  getStr += apiKey;
  getStr += "&field1=";
  getStr += id;
  getStr += "\r\n";
  //Serial.println("id:");
  //Serial.println(id);

  // Send data length & GET string
  ESP01.print("AT+CIPSEND=");
  ESP01.println (getStr.length());
  Serial.print("AT+CIPSEND=");
  Serial.println (getStr.length());
  delay(500);
  if ( ESP01.find( ">" ) )
  {
    Serial.print(">");
    sendData(getStr, 2000, DEBUG);
  }

  // Close connection, wait a while before repeating...
  sendData("AT+CIPCLOSE", 16000, DEBUG); // thingspeak needs 15 sec delay between updates
  ultrasonicRanger();
}

// this method is called when fingerprint is not matched
void fingerprintFail() {
  wrong++;
  Serial.println(F("Wrong:"));
  Serial.println(wrong);
  // when user scanned fingerprint not match more than 3 times then 'disable' the arduino, if not display error message
  if (wrong <= 3) {
    // display error message on lcd
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Fingerprint"));
    lcd.setCursor(0, 1);
    lcd.print(F("not matched!"));
    // short buzzer sound to alert user that fingerprint is wrong
    tone(8, 500, 500);
    delay(2000);
    // clear lcd message
    lcd.clear();
  } else {
    // display warning message
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Intruder"));
    lcd.setCursor(0, 1);
    lcd.print(F("Alert!"));
    tone(8, 500, 500);
    delay(1000);
    tone(8, 500, 500);
    delay(1000);
    tone(8, 500, 500);
    delay(1000);
    // clear lcd message
    lcd.clear();
    lcd.noBacklight();

    // send alert notification through twitter
    Serial.println();
    sendData("AT+RST\r\n", 2000, DEBUG);
    sendData("AT+CWMODE=1\r\n", 2000, DEBUG);
    //sendData("AT+CWJAP=\"Family2.Net\",\"chuleephing7\"\r\n", 4000, DEBUG); // kelyn's home wifi
    sendData("AT+CWJAP=\"Kelyn IX\",\"onetonine\"\r\n", 4000, DEBUG);
    sendData("AT+CIPMUX=0\r\n", 2000, DEBUG);

    // message to notify
    String temp = "Intruder alert!";

    // TCP connection
    String cmd = "AT+CIPSTART=\"TCP\",\"";
    cmd += "184.106.153.149"; // api.thingspeak.com
    cmd += "\",80\r\n";
    sendData(cmd, 2000, DEBUG);

    if (ESP01.find("Error")) {
      Serial.println("AT+CIPSTART error");
      return;
    }

    String tsData = "api_key=" APIKEY "&status=" + temp ;

    // prepare GET string
    String getStr = "POST /apps/thingtweet/1/statuses/update HTTP/1.1\n";
    getStr += "Host: api.thingspeak.com\n";
    getStr += "Connection: close\n";
    getStr += "Content-Type: application/x-www-form-urlencoded\n";
    getStr += "Content-Length: ";
    getStr += tsData.length();
    getStr += "\n\n";
    getStr += tsData;

    // send data length
    ESP01.print("AT+CIPSEND=");
    ESP01.println (getStr.length());

    if (ESP01.find(">")) {
      sendData(getStr, 2000, DEBUG);
      Serial.println("nice");
    }
    else {
      sendData("AT+CIPCLOSE", 16000, DEBUG);
      Serial.println("AT+CIPCLOSE");
    }
    delay(16000);



    // 'disable' arduino
    delay(10000);
    // call reset wrong method
    resetWrong();
    ultrasonicRanger();
  }
}

// this method is called to activate ultrasonic ranger to detect motion
void ultrasonicRanger() {
  //count2--;
  //Serial.println(F("count2:"));
  //Serial.println(count2);
  
  digitalWrite(Trig, LOW);
  delayMicroseconds(2);
  digitalWrite(Trig, HIGH);
  delayMicroseconds(5);
  digitalWrite(Trig, LOW);

  duration = pulseIn(Echo, HIGH);
  cm = duration / 58;
  delay(500);
  // when detect motion in range, reset countdown
  if (cm <= 10) {
    resetCount();
  } else {
    loop();
  }
}

// reset count
void resetCount() {
  count = 0;
}

// reset count2
//void resetCount2() {
//  count2 = 20;
//}

// reset wrong
void resetWrong() {
  wrong = 0;
}


// for ESP01 to send data to thingspeak
String sendData(String command, const int timeout, boolean debug)
{
  String response = "";
  ESP01.print(command);
  long int time = millis();

  while ( (time + timeout) > millis())
  {
    while (ESP01.available())
    {
      // "Construct" response from ESP01 as follows
      // - this is to be displayed on Serial Monitor.
      char c = ESP01.read(); // read the next character.
      response += c;
    }
  }

  if (debug)
  {
    Serial.print(response);
  }

  return (response);
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println(F("Image taken"));
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(F("No finger detected"));
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println(F("Communication error"));
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println(F("Imaging error"));
      return p;
    default:
      Serial.println(F("Unknown error"));
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println(F("Image converted"));
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println(F("Image too messy"));
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println(F("Communication error"));
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println(F("Could not find fingerprint features"));
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println(F("Could not find fingerprint features"));
      return p;
    default:
      Serial.println(F("Unknown error"));
      return p;
  }

  // OK converted!
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println(F("Found a print match!"));
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println(F("Communication error"));
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println(F("Did not find a match"));
    return p;
  } else {
    Serial.println(F("Unknown error"));
    return p;
  }

  // found a match!
  Serial.print(F("Found ID #")); Serial.print(finger.fingerID);
  Serial.print(F(" with confidence of ")); Serial.println(finger.confidence);

  return finger.fingerID;
}

int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;
  // when fingerprint not matched, return 0
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return 0;

  // found a match!
  Serial.print(F("Found ID #")); Serial.print(finger.fingerID);
  Serial.print(F(" with confidence of ")); Serial.println(finger.confidence);
  if (finger.confidence > 150) {
    id = finger.fingerID;
    return 1;
  } else if (finger.confidence > 0 && finger.confidence <= 150) {
    return 0;
  } else {
    return -1;
  }
}

void getMessage() {
  //check if there is any message incoming
  Serial.println();
  sendData("AT+RST\r\n", 2000, DEBUG);
  sendData("AT+CWMODE=1\r\n", 2000, DEBUG);
  //sendData("AT+CWJAP=\"Family2.Net\",\"chuleephing7\"\r\n", 4000, DEBUG);
  sendData("AT+CWJAP=\"Kelyn IX\",\"onetonine\"\r\n", 4000, DEBUG);
  sendData("AT+CIPMUX=0\r\n", 2000, DEBUG);

  // Make TCP connection
  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += "184.106.153.149"; // Thingspeak.com's IP address
  cmd += "\",80\r\n";
  sendData(cmd, 2000, DEBUG);

  // Prepare GET string
  String getStr = "GET /channels/1091956/status/last.json\r\n";

  if (ESP01.find("Error")) {
    Serial.println("AT+CIPSTART error");
    return;
  }

  // Send data length & GET string
  ESP01.print("AT+CIPSEND=");
  ESP01.println (getStr.length());
  Serial.print("AT+CIPSEND=");
  Serial.println (getStr.length());
  delay(500);

  if ( ESP01.find( ">" ) ) {
    Serial.print(">");
    String reply = sendData (getStr, 2000, DEBUG);
    reply.remove(reply.length() - 8);
    reply.remove(0, 39);
    Serial.println(F("reply:"));
    Serial.println(reply);

    const size_t capacity = JSON_OBJECT_SIZE(3) + 70;
    DynamicJsonBuffer jsonBuffer(capacity);
    JsonObject& root = jsonBuffer.parseObject(reply);

    // Test if parsing succeeds.
    if (!root.success()) {
      Serial.println("parseObject() failed");
      return;
    }

    message = root["status"];
    Serial.println(message);
  }

  // Close connection, wait a while before repeating...
  sendData("AT+CIPCLOSE", 16000, DEBUG); // thingspeak needs 15 sec delay between updates
}

void getDoor() {
  // retriving from field 2 to check if user wants door to be unlock/lock
  // unlock=1, lock=0
  Serial.println();
  sendData("AT+RST\r\n", 2000, DEBUG);
  sendData("AT+CWMODE=1\r\n", 2000, DEBUG);
  //sendData("AT+CWJAP=\"Family2.Net\",\"chuleephing7\"\r\n", 4000, DEBUG);
  sendData("AT+CWJAP=\"Kelyn IX\",\"onetonine\"\r\n", 4000, DEBUG);
  sendData("AT+CIPMUX=0\r\n", 2000, DEBUG);

  // Make TCP connection
  String cmd2 = "AT+CIPSTART=\"TCP\",\"";
  cmd2 += "184.106.153.149"; // Thingspeak.com's IP address
  cmd2 += "\",80\r\n";
  sendData(cmd2, 2000, DEBUG);

  // Prepare GET string
  String getStr2 = "GET /channels/1091956/fields/2/last.json\r\n";

  if (ESP01.find("Error")) {
    Serial.println("AT+CIPSTART error");
    return;
  }

  // Send data length & GET string
  ESP01.print("AT+CIPSEND=");
  ESP01.println (getStr2.length());
  Serial.print("AT+CIPSEND=");
  Serial.println (getStr2.length());
  delay(500);

  if ( ESP01.find( ">" ) ) {
    Serial.print(">");
    String reply = sendData (getStr2, 2000, DEBUG);
    reply.remove(reply.length() - 8);
    reply.remove(0, 39);
    Serial.println(F("reply:"));
    Serial.println(reply);

    const size_t capacity = JSON_OBJECT_SIZE(3) + 60;
    DynamicJsonBuffer jsonBuffer(capacity);
    JsonObject& root = jsonBuffer.parseObject(reply);

    // Test if parsing succeeds.
    if (!root.success()) {
      Serial.println("parseObject() failed");
      return;
    }

    door = root["field2"];
    Serial.println(door);
  }

  // Close connection, wait a while before repeating...
  sendData("AT+CIPCLOSE", 16000, DEBUG); // thingspeak needs 15 sec delay between updates
}

void getArduino() {
  // retriving from field 3 to check if user wants arduino to be on/off
  // on=1, off=0
  Serial.println();
  sendData("AT+RST\r\n", 2000, DEBUG);
  sendData("AT+CWMODE=1\r\n", 2000, DEBUG);
  //sendData("AT+CWJAP=\"Family2.Net\",\"chuleephing7\"\r\n", 4000, DEBUG);
  sendData("AT+CWJAP=\"Kelyn IX\",\"onetonine\"\r\n", 4000, DEBUG);
  sendData("AT+CIPMUX=0\r\n", 2000, DEBUG);

  // Make TCP connection
  String cmd3 = "AT+CIPSTART=\"TCP\",\"";
  cmd3 += "184.106.153.149"; // Thingspeak.com's IP address
  cmd3 += "\",80\r\n";
  sendData(cmd3, 2000, DEBUG);

  // Prepare GET string
  String getStr3 = "GET /channels/1091956/fields/3/last.json\r\n";

  if (ESP01.find("Error")) {
    Serial.println("AT+CIPSTART error");
    return;
  }

  // Send data length & GET string
  ESP01.print("AT+CIPSEND=");
  ESP01.println (getStr3.length());
  Serial.print("AT+CIPSEND=");
  Serial.println (getStr3.length());
  delay(500);

  if ( ESP01.find( ">" ) ) {
    Serial.print(">");
    String reply = sendData (getStr3, 2000, DEBUG);
    reply.remove(reply.length() - 8);
    reply.remove(0, 39);
    Serial.println(F("reply:"));
    Serial.println(reply);

    const size_t capacity = JSON_OBJECT_SIZE(3) + 60;
    DynamicJsonBuffer jsonBuffer(capacity);
    JsonObject& root = jsonBuffer.parseObject(reply);

    // Test if parsing succeeds.
    if (!root.success()) {
      Serial.println("parseObject() failed");
      return;
    }

    arduino = root["field3"];
    Serial.println(arduino);
  }

  // Close connection, wait a while before repeating...
  sendData("AT+CIPCLOSE", 16000, DEBUG); // thingspeak needs 15 sec delay between updates
}

void setup()
{
  Serial.begin(9600);
  pinMode(Trig, OUTPUT);
  pinMode(Echo, INPUT);
  myservo.attach(9);
  // set locked door at the start
  myservo.write(90);
  while (!Serial);
  delay(100);

  ESP01.begin(9600);
  lcd.init();
}

void loop()
{
//  if (count2 == 0) {
//    getArduino();
//    if(arduino != 0){
      getMessage();
//      getDoor();
//    }
//    resetCount2();
//  }else{
      if (arduino == 1) {
      if (door == 1) {
        // unlock the door
        myservo.write(0);
        // play the melody
        for (int thisNote = 0; thisNote < 8; thisNote++) {
          int noteDuration = 1000 / noteDurations[thisNote];
          tone(8, melody[thisNote], noteDuration);
  
          int pauseBetweenNotes = noteDuration * 1.30;
          delay(pauseBetweenNotes);
  
          noTone(8);
          door = -1;
        }
      } else if (door == 0) {
        myservo.write(90);
      }
      if (strlen(message) != 0) {
        lcd.backlight();
        lcd.setCursor(0, 0);
        lcd.print(message);
        delay(5000);
        lcd.clear();
      }
      // when untrasonic ranger dectects motion within range then proceed to display 'scan finger' message, if not continue to detect if there is motion
      if (cm <= 10 && count <= 10) {
        // display message
        lcd.backlight();
        lcd.setCursor(0, 0);
        lcd.print(F("Please scan"));
        lcd.setCursor(0, 1);
        lcd.print(F("your finger."));
        // set the data rate for the sensor serial port
        finger.begin(57600);
        int fingerprint;
        // call method and returned with either 1, 0 or -1
        fingerprint = getFingerprintIDez();
        Serial.println(F("fingerprint:"));
        Serial.println(fingerprint);
        // if fingerprint = 1, it success
        // if fingerprint = 0, its fail
        // if fingerprint = -1, do nothing and continue to wait for finger to be scanned
        if (fingerprint == 1) {
          fingerprintSuccess();
        } else if (fingerprint == 0) {
          fingerprintFail();
        }
        delay(100);
        lcd.clear();
        //Serial.println(F("count:"));
        //Serial.println(count);
        count++;
      } else {
        ultrasonicRanger();
      }
    }
  //}
}
