//==================================================================
//LIBRARY LIBRARY LIBRARY LIBRARY LIBRARY LIBRARY LIBRARY LIBRARY
//==================================================================
#include <Wire.h>
#include <ThingerESP32.h>
#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <SPI.h>
#include <LoRa.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


//==================================================================
//LORA SETUP || LORA SETUP || LORA SETUP || LORA SETUP || LORA SETUP
//==================================================================
const int csPin = 5;        
const int resetPin = 14;      
const int irqPin = 2;         

String outgoing;            
byte msgCount = 0;          
byte localAddress = 0xBA;   
byte destination = 0xBB;   
long lastSendTime = 0;  


//==================================================================
//VARIABEL GLOBAL || VARIABEL GLOBAL || VARIABEL GLOBAL
//==================================================================
String kondisi;
int led = 0;
String incoming; 
//#define buzzer 33


//==================================================================
//DEFINISI THINGER || DEFINISI THINGER || DEFINISI THINGER ||
//==================================================================
#define USERNAME "mawarizkar"
#define DEVICE_ID "ESP32MAWAR"
#define DEVICE_CREDENTIAL "R3CzY6NAFl2c"


//==================================================================
//DEFINISI USER DAN PASSWORD WIFI / HOTSPOT / ACCESS POINT
//==================================================================
#define SSID "RizHP"
#define SSID_PASSWORD "b4rulagi1"


//==================================================================
//INISIALISASI TINGER || INISIALISASI TINGER || INISIALISASI TINGER
//==================================================================
ThingerESP32 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);


//==================================================================
//SETTING GPS || SETTING GPS || SETTING GPS || SETTING GPS
//==================================================================
static const uint32_t GPSBaud = 9600;
HardwareSerial MySerial(1);
TinyGPSPlus gps;
double latitude, longitude;


//==================================================================
//PARAMETER DAN VARIABEL GYRO || PARAMETER DAN VARIABEL GYRO
//==================================================================
const int MPU_addr = 0x68; // I2C address of the MPU-6050
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;


//==================================================================
//TELEGRAM SET UP || TELEGRAM SET UP || TELEGRAM SET UP
//==================================================================
char ssid[] = SSID;     
char password[] = SSID_PASSWORD; 
#define BOTtoken "937990287:AAG8gCWnOTmoCzE-581u4fIyvNUveQVNA3w"
#define CHAT_ID "813574278"
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
int Bot_mtbs = 1000; 
long Bot_lasttime;   
volatile bool telegramFlag = false;


//==================================================================
//SETUP GLOBAL || SETUP GLOBAL || SETUP GLOBAL || SETUP GLOBAL
//==================================================================
void setup() 
{
  //pinMode(buzzer, OUTPUT);
  
  //****************************************************************
  //MEMBUAT KONEKSI & MENGIRIMKAN VARIABEL KE THINGER.IO 
  //****************************************************************
  thing.add_wifi(SSID, SSID_PASSWORD);
  thing["KondisiMobil"] >> outputValue(kondisi);
  thing["Led"] >> outputValue(led);
  thing["Location"] >> [](pson & out)
  {
    out["lat"] = gps.location.lat();
    out["long"] = gps.location.lng();
  };
  
  //****************************************************************
  //MEMULAI GPS || MEMULAI GPS || MEMULAI GPS || MEMULAI GPS ||
  //****************************************************************
  Serial.begin(9600);
  MySerial.begin(9600, SERIAL_8N1, 16, 17);
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0); // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
  
  //****************************************************************
  //MEMULAI LORA || MEMULAI LORA || MEMULAI LORA || MEMULAI LORA ||
  //****************************************************************
  LoRa.setPins(csPin, resetPin, irqPin);
  if (!LoRa.begin(433E6)) 
  {             
    Serial.println("LoRa init failed. Check your connections.");
    while (true);                    
  }
  LoRa.onReceive(onReceive);
  LoRa.receive();
  Serial.println("LoRa init succeeded.");
}
//==================================================================
//PROGRAM UTAMA || PROGRAM UTAMA || PROGRAM UTAMA || PROGRAM UTAMA
//==================================================================
void loop()
{
  //****************************************************************
  //KIRIM VARIABEL KE THINGER.IO
  //****************************************************************
  thing.handle();
  
  //****************************************************************
  //MEMULAI PEMBACAAN GYRO
  //****************************************************************
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 14, true);
  
  AcX = Wire.read() << 8 | Wire.read();
  AcY = Wire.read() << 8 | Wire.read();
  AcZ = Wire.read() << 8 | Wire.read();
  Tmp = Wire.read() << 8 | Wire.read();
  GyX = Wire.read() << 8 | Wire.read();
  GyY = Wire.read() << 8 | Wire.read();
  GyZ = Wire.read() << 8 | Wire.read();
  
  //****************************************************************
  //MEMULAI PEMBACAAN GPS
  //****************************************************************
  while (MySerial.available() > 0)
  {
    gps.encode(MySerial.read());
    if (gps.location.isUpdated())
    {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
      Serial.print(latitude, 6);
      Serial.print(" ; ");
      Serial.println(longitude, 6);
    }
  }
  
  //****************************************************************
  //SELEKSI KONDISI KECELAKAAN
  //****************************************************************
  if ((AcX<1000 && AcY<-8000) || (AcX<1000 && AcY>8000) || (AcX>8000 && AcY<1000) || (AcX<-8000 && AcY<1000))
  {
    led = 1;
    kondisi = "Mobil ID: 001 mengalami KECELAKAAN";
    telegramFlag = true;
    sendTelegramMessage();
    //String message = (""+ String(led));
    String message = ("#");
    message.concat(led);
    message.concat(";");
    message.concat(""+ String(latitude, 6));  
    message.concat(";");
    message.concat(""+ String(longitude, 6)); 
    sendMessage(message);
    Serial.println("Sending " + message);
    LoRa.receive();
    Serial.println("KECELAKAAN");
  }
  else
  {
    led = 0;
    kondisi = "Mobil ID: 001 dalam keadaan NORMAL";
    LoRa.receive();
    Serial.println("AMAN");
  }

  //****************************************************************
  //TELEGRAM LISTEN MESSAGE FROM USER / REQUEST BY USER
  //****************************************************************
  if (millis() > Bot_lasttime + Bot_mtbs)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      //Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    Bot_lasttime = millis();
  }
  delay(1000);


  //****************************************************************
  //TELEGRAM LISTEN MESSAGE FROM Lora / TRIGGER BY LoRa
  //****************************************************************
  if (incoming == "1")
  {
    Serial.println("LoRa Receive to send telegram to close car");
    telegramFlag = true;
    sendTelegramMessage2();
    incoming = "0";
  }
}


//==================================================================
//FUNCTION LORA sendMessage || FUNCTION LORA sendMessage
//==================================================================
void sendMessage(String outgoing) {
  LoRa.beginPacket();                   // start packet
  LoRa.write(destination);              // add destination address
  LoRa.write(localAddress);             // add sender address
  LoRa.write(msgCount);                 // add message ID
  LoRa.write(outgoing.length());        // add payload length
  LoRa.print(outgoing);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  msgCount++;                           // increment message ID
}


//==================================================================
//FUNCTION LORA onReceive || FUNCTION LORA onReceive
//==================================================================
void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();            // sender address
  byte incomingMsgId = LoRa.read();     // incoming msg ID
  byte incomingLength = LoRa.read();    // incoming msg length

  incoming = "";                 // payload of packet

  while (LoRa.available()) {            // can't use readString() in callback, so
    incoming += (char)LoRa.read();      // add bytes one by one
  }

  if (incomingLength != incoming.length()) {   // check length for error
    Serial.println("error: message length does not match length");
    return;                           
  }

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress && recipient != 0xFF) {
    Serial.println("This message is not for me.");
    return;                            
  }

  // if message is for this device, or broadcast, print details:
  Serial.println("FROM: 0x" + String(sender, HEX));
  Serial.println("Message: " + incoming);
  Serial.println();
}

//==================================================================
//FUNCTION: TELEGRAM HANDLE REQUEST BY USER
//==================================================================
void handleNewMessages(int numNewMessages)
{
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "") from_name = "Guest";

    if (text == "/status") {
      //String link = "http://www.google.com/maps/place/" + String(latitude, 6) + "," + String(longitude, 6);
      String link = "http://www.google.com/maps/place/-6.1830234,106.8282463";
      if (led) {
        bot.sendMessage(chat_id, "Mobil Anda mengalami KECELAKAAN!\n\nSilahkan klik link berikut untuk mengetahui posisi mobil anda\n" + String(link));
      } else {
        bot.sendMessage(chat_id, "Mobil Anda dalam keadaan NORMAL\n\nSilahkan klik link berikut untuk mengetahui posisi mobil anda " + String(link));
      }
    }

    if (text == "/start") {
      String welcome = "Welcome to Automatic Emergency Report System, " + from_name + ".\n";
      welcome += "with this system you can find the current condition of your car and track the last position of your car.\n\n";
      welcome += "/status : To get your car condition and location\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}


//==================================================================
//FUNCTION: TELEGRAM SEND MESSAGE TRIGGER BY DEVICE
//==================================================================
void sendTelegramMessage()
{
  String link2 = "http://www.google.com/maps/place/" + String(latitude, 6) + "," + String(longitude, 6);
  //String link2 = "http://www.google.com/maps/place/-6.1830234,106.8282463";
  String message = "Mobil keluarga Anda (ID: 001) mengalami KECELAKAAN!";
  //String message = "Ada mobil di dekat Anda (ID: 001) yang sedang mengalami KECELAKAAN";
  message.concat("\n");
  message.concat("\n");
  message.concat("Silahkan klik link berikut untuk mengetahui posisi mobil");
  message.concat("\n");
  message.concat(link2);
  //bot.sendMessage(CHAT_ID, " \n\n ");
  if (bot.sendMessage(CHAT_ID, message, "Markdown")) {
    Serial.println("TELEGRAM Trigger by Device");
  }
  telegramFlag = false;
}


//==================================================================
//FUNCTION: TELEGRAM SEND MESSAGE TRIGGER BY LORA
//==================================================================
void sendTelegramMessage2()
{
  String link2 = "http://www.google.com/maps/place/" + String(latitude, 6) + "," + String(longitude, 6);
  //String link3 = "http://www.google.com/maps/place/-6.1830234,106.8282463";
  String message = "Ada mobil di dekat Anda yang sedang mengalami KECELAKAAN";
  message.concat("\n");
  message.concat("\n");
  message.concat("Silahkan klik link berikut untuk mengetahuinya");
  message.concat("\n");
  message.concat(link2);
  //bot.sendMessage(CHAT_ID, " \n\n ");
  if (bot.sendMessage(CHAT_ID, message, "Markdown")) {
    Serial.println("TELEGRAM LORA SUCCESS");
  }
  telegramFlag = false;
}
