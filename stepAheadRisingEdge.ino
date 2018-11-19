#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
#include <SPI.h>
#include <RFID.h>
#include <Ethernet.h>
#include <TimeLib.h>

#define TEMPERATURE_CONVERSION_RATE 500/1024 //500mv for 1024 
#define TEMPERATURE_SAMPLE_RATE 1000

#define BACKLIGHT 0x7

#define SDAPIN 9 // RFID Module SDA pin
#define RESETPIN 8 // RFID Module RESET pin
#define TEAMID 377177
const int sensorTemperature = A0; // analog pin used to measure Temperature
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
RFID nfc(SDAPIN, RESETPIN);

time_t timeOnSelect;


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
  pinMode(sensorTemperature, INPUT);
  Serial.begin(9600);
  lcd.begin(16, 2);
  SPI.begin();
  nfc.init();

  lcd.setBacklight(BACKLIGHT);
  bookingTime = 0;
  roomEmpty = true;


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
  //Serial.print(readTemperature());
  //Serial.print("C");
  //Serial.println("");
  //Serial.println(String(readTemperature()));
  

  //writeToDisplay(readTemperature());
   if (roomEmpty) {
     
     AddData(String(TEAMID), "NA" ,"NA", String(readTemperature()));
    
   }

  
  printRoomState();
  if (roomEmpty) {
    readCard();
  }
  delay(1000);







}


float readTemperature() {
  //delay(TEMPERATURE_SAMPLE_RATE);
  float tCelsius, voltage;
  voltage = analogRead(sensorTemperature);
  tCelsius = voltage * TEMPERATURE_CONVERSION_RATE;
  return tCelsius;
}
//
//void writeToDisplay(float value) {
//  lcd.clear();
//  lcd.setCursor(0, 0);
//
//  lcd.print(value);
//}

void readCard() {
  if (nfc.isCard())
  {
    /* If so then get its serial number */
    nfc.readCardSerial();
    Serial.println("Card detected:");
    String serialNumber = "";
    for (int i = 0; i < 5; i++)
    {
      serialNumber = serialNumber + nfc.serNum[i];
    }
    Serial.println();
    Serial.println(CheckCardId(serialNumber));
    Serial.println(serialNumber);
    Serial.println();
    Serial.println();
    if (CheckCardId(serialNumber)) {
      setTime();
    }

  }
}


void setTime() {
  byte flagSelect = 1;
  lcd.clear();
  lcd.setCursor(5, 1);
  lcd.print(String(bookingTime / 60) + ":" + String(bookingTime % 60) + "0");
  while (flagSelect) {
    uint8_t buttons = lcd.readButtons();
    if (buttons) {
      if (buttons & BUTTON_UP) {
        delay(300);
        bookingTime = bookingTime + 15 * 60 ; //add 15 minutes

      }
      if (buttons & BUTTON_DOWN) {
        if (bookingTime != 0) {
          delay(300);
          bookingTime = bookingTime - 15 * 60; //subtract 15 minutes

        }
      }
      if (buttons & BUTTON_SELECT) {
        delay(300);
        flagSelect = 0;
        roomEmpty = false;
        timeOnSelect = now();

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
      timeOnSelect = currentTime;
    } else {
      roomEmpty = true;
      bookingTime = 0;
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
    delay(1500);

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

    delay(1500);

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
