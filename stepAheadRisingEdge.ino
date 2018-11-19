#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
#include <SPI.h>
#include <RFID.h>
#include <Ethernet.h>
#include <TimeLib.h>

#define VOLTAGE_CONVERSION_RATE 500/1024 //500mv for 1024 
#define TEMPERATURE_SAMPLE_RATE 1000

#define BACKLIGHT 0x7

#define SDAPIN 9 // RFID Module SDA pin
#define RESETPIN 8 // RFID Module RESET pin
#define TEAMID 377177  //team id
const int sensorTemperature = A0; // analog pin used to measure Temperature
const int sensorIRFirst = A1; // analog pin used for the IR sensor 
const int sensorIRSecond = A2; // analog pin used for the IR sensor 
/*In this implementation one of the sensors is put on the ground at 45 degrees and the other one is put on the wall*/

int distanceIRFirst; //OX inistial setup distance
int distanceIRSecond;  //angle inistial setup distance
int numberOfPeople; // number of people in the meeting room ( that crossed the door)
int maxIncrement;

String serialNumber; // serial number of the card


Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
RFID nfc(SDAPIN, RESETPIN);

time_t timeOnSelect; // time when the room was occupied
time_t timeOnDrop; // time since when the room was emptied
time_t lastSend;
EthernetClient client;
IPAddress ip(192, 168, 0, 52);
IPAddress myDns(192, 168, 0, 1);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
byte server[] = {192, 168,   0,  5};
byte mac[] = {
  0x88, 0x14, 0x56, 0xB1, 0xA4, 0x14
};

int bookingTime; //time in seconds
bool roomEmpty; // false - occupied , true -  free


void setup() {
  //pinMode(sensorTemperature, INPUT);
  Serial.begin(9600);
  lcd.begin(16, 2);
  SPI.begin();
  nfc.init();

  lcd.setBacklight(BACKLIGHT);
  bookingTime = 0;
  roomEmpty = true;
  distanceIRFirst = readDistanceSensor(sensorIRFirst);
  distanceIRSecond = readDistanceSensor(sensorIRSecond);
  numberOfPeople = 0;
  maxIncrement = 0;
  serialNumber = "";
  lastSend = now() - 10 ;
  Ethernet.begin(mac, ip, myDns, gateway, subnet);
  // Check connection to web service
  bool result = TestConnection();
  if (result) {
    Serial.println("Connection with Web Service is up");
  } else {
    Serial.println("Connection with Web Service is down");
  }

}

void loop() {



  if (roomEmpty) {

    sendData("NA", "NA");

  } else {
    sendData(serialNumber, String(numberOfPeople));

    int measure = 0;
    int crossedIRFirst = 0;
    int crossedIRSecond = 0;
    //in this configuration both sensors are on the wall , if you cross the first sensor and then the second one you leave the room otherwise you are entering it
    while ( measure < 10) {
      delay(50);
      measure++;
      if (readDistanceSensor(sensorIRFirst) != distanceIRFirst) {
        crossedIRFirst = crossedIRFirst + crossedIRSecond + 1;
      }
       if (readDistanceSensor(sensorIRSecond) != crossedIRSecond) {
        crossedIRSecond = crossedIRFirst + crossedIRSecond + 1;
      }
    }

    if (crossedIRFirst != 0 &&  crossedIRSecond != 0) {

      if (crossedIRFirst > crossedIRSecond) {
        if (numberOfPeople > 0) {
          numberOfPeople --;
        }

      } else {
        numberOfPeople ++;
      }
    }
    if (numberOfPeople == 0) {
      time_t currentTime = now();
      if ( currentTime - timeOnDrop >= 300) {
        roomEmpty = true;
      }
    } else {
      timeOnDrop = now();
    }
    extendTime();

  }


  printRoomState();
  if (roomEmpty) {
    readCard();
  }


  delay(1000);







}
void sendData(String serialNumber, String numberOfPeople) {
  if (now() - lastSend > 10) {
    int tCelsius = int(readTemperature());
    AddData(String(TEAMID), serialNumber , numberOfPeople, String(tCelsius));
    lastSend = now();
  }
}


float readTemperature() {
  //delay(TEMPERATURE_SAMPLE_RATE);
  float tCelsius, voltage;
  voltage = analogRead(sensorTemperature);
  tCelsius = voltage * VOLTAGE_CONVERSION_RATE;
  return tCelsius;
}

float readDistanceSensor(int sensorPin) {
  float  voltage, distance;
  voltage = analogRead(sensorPin) * 0.0048828125;
  distance = 13 * pow(voltage, -1);
  if (distance > 30) {
    distance = 30;
  }
  Serial.println(distance);
  return distance;
}


void readCard() {
  if (nfc.isCard())
  {
    /* If so then get its serial number */
    nfc.readCardSerial();
    Serial.println("Card detected:");
    serialNumber = "";
    for (int i = 0; i < 5; i++)
    {
      serialNumber = serialNumber + nfc.serNum[i];
    }
    Serial.println(serialNumber);
    Serial.println();
    if (CheckCardId(serialNumber)) {
      setTime();
    }

  }
}


void setTime() {

  lcd.clear();
  lcd.setCursor(5, 1);
  lcd.print(String(bookingTime / 60) + ":" + String(bookingTime % 60) + "0");
  while (roomEmpty) {
    uint8_t buttons = lcd.readButtons();
    if (buttons) {
      if (buttons & BUTTON_UP) {
        delay(300);
        bookingTime = bookingTime + 15 * 60; //add 15 minutes

      }
      if (buttons & BUTTON_DOWN) {
        if (bookingTime != 0) {
          delay(300);
          bookingTime = bookingTime - 15 * 60; //subtract 15 minutes

        }
      }
      if (buttons & BUTTON_SELECT) {
        delay(300);
        roomEmpty = false;
        timeOnSelect = now();
        timeOnDrop = now();
        sendData(serialNumber, String(numberOfPeople));
      }
      lcd.clear();
      lcd.setCursor(5, 1);
      lcd.print(String(bookingTime / 60) + ":" + String(bookingTime % 60) + "0");
    }
  }
  // function call to  server
}



void printRoomState() {
  if (roomEmpty) {
    lcd.clear();
    lcd.setCursor(5, 0);
    lcd.print("Free");
  } else {
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print("Occupied");
    lcd.setCursor(5, 1);
    lcd.print(String(bookingTime / 60) + ":" + String(bookingTime % 60));
    time_t currentTime = now();
    if (bookingTime > 0 ) {
      bookingTime = bookingTime - (currentTime - timeOnSelect);
      if (bookingTime < 0 ) {
        bookingTime = 0;
      }
      timeOnSelect = currentTime;
    } else {
      roomEmpty = true;
      bookingTime = 0;
    }
  }
}


void extendTime() {

  uint8_t buttons = lcd.readButtons();

  if (buttons && maxIncrement <= 4) {
    if (buttons & BUTTON_UP && maxIncrement < 4) {
      delay(300);
      bookingTime = bookingTime + 15 * 60; //add 15 minutes
      maxIncrement ++;

    }
    if (buttons & BUTTON_DOWN && maxIncrement > 0) {
      if (bookingTime > 0) {
        delay(300);
        bookingTime = bookingTime - 15 * 60; //subtract 15 minutes
        maxIncrement --;
      }
    }
  }

}




boolean TestConnection() {
  String response = "";
  client.connect(server, 80);

  if (client.connected()) {
    client.println("POST /MeetingRoomTracker/MeetingRoomTracker.asmx/TestConnection HTTP/1.1");
    client.println("Host: localhost");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: "); client.println(0);

    client.println();
    delay(1000);

    while (client.available())
    {
      char c = client.read();
      response = String(response + c);
    }
  }
  client.stop();
  client.flush();
  //Print HTTP response content for debug purpose.
  //Serial.println(response);
  //Extract only the response if available.
  return response.indexOf("true") > 0;
}

boolean CheckCardId(String CardID) {
  client.connect(server, 80);

  String value = String("userId=" + CardID);
  String response = "";
  if (client.connected()) {
    client.println("POST /MeetingRoomTracker/MeetingRoomTracker.asmx/ValidateUser HTTP/1.1");
    client.println("Host: localhost");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: "); client.println(value.length());
    client.println();
    client.println(value);
    client.println();
    delay(1000);

    while (client.available())
    {
      char c = client.read();
      response = String(response + c);

    }
  }
  client.stop();
  client.flush();
  //Print HTTP response content for debug purpose.
  //Serial.println(response);
  return response.indexOf("true") > 0;
}

void AddData(String teamId, String userId, String numberOfParticipants, String temperature) {
  client.connect(server, 80);

  String teamValue = String("teamId=" + teamId);
  String cardValue = String("&userId=" + userId);
  String participantsValue = String("&numberOfParticipants=" + numberOfParticipants);
  String temperatureValue = String("&temperature=" + temperature);
  String content = String(teamValue + cardValue + participantsValue + temperatureValue);

  String response = "";
  if (client.connected()) {
    client.println("POST /MeetingRoomTracker/MeetingRoomTracker.asmx/AddData HTTP/1.1");
    client.println("Host: localhost");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: "); client.println(content.length());
    client.println();
    client.println(content);
    client.println();

    delay(1000);

    while (client.available())
    {
      char c = client.read();
      response = String(response + c);
    }
  }
  client.stop();
  client.flush();
  //Print HTTP response content for debug purpose.
  //Serial.println(response);


}
