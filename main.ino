#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <DHT.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID "imd0902"
#define WIFI_PASSWORD "imd0902iot"
#define API_KEY "AIzaSyCg7DEZ5cCK4X2WrCskOBKgKcrXT6JfFNQ"
#define DATABASE_URL "https://smart-room-manager-database-default-rtdb.firebaseio.com/"

#define LED1 27
#define LED2 26
#define LED3 25
#define DHTPIN 4
#define DHTTYPE DHT11   // DHT 11

#define MSG_BUFFER_SIZE  (50)
#define RST_PIN         16        
#define SS_PIN          17        
#define IO_USERNAME  "danielnobre09"
#define IO_KEY       "aio_VjeQ31W9o1xY5JKjQMhOMUKe6Qvt"

const char* mqttserver = "io.adafruit.com";
const int mqttport = 1883;
const char* mqttUser = IO_USERNAME;
const char* mqttPassword = IO_KEY;

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
char msg[MSG_BUFFER_SIZE];
MFRC522 mfrc522(SS_PIN, RST_PIN);

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
String lastTagID = "";
bool isCardPresent = false;

// Variáveis do firebase
bool boolIsOn;
int intTemp;
bool boolLed1;
bool boolLed2;
bool boolLed3;
String stringName;

// Strings do usuário
String tagName;
String tagTemp;
String tagIsOn;
String tagLed1;
String tagLed2;
String tagLed3;

bool signupOK = false;

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    // Create a random client ID
    String clientId = "ESP32 - Sensores";
    clientId += String(random(0xffff), HEX);
    // Se conectado
    if (client.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("conectado");
      // Depois de conectado, publique um anúncio ...
      client.publish("danielnobre09/feeds/Nome", "Iniciando Comunicação");
      client.publish("danielnobre09/feeds/Temperatura", "Iniciando Comunicação");
      client.publish("danielnobre09/feeds/Umidade", "Iniciando Comunicação");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5s");
      delay(500);
    }
  }
}

void turnOffLeds() {
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
}

void setup() {
  dht.begin();
  SPI.begin();                      // Initialize SPI bus
  mfrc522.PCD_Init();               // Initialize MFRC522 RFID module
  Serial.println("Ready to read RFID tags...");
  Serial.begin(115200);

  lcd.begin();
  lcd.backlight();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  client.setServer(mqttserver, 1883); // Publicar

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  delay(1000);
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    isCardPresent = true;

    String tagID = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      tagID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
      tagID += String(mfrc522.uid.uidByte[i], HEX);
    }

    if (lastTagID != tagID) {
      lcd.clear();
      lcd.print("Conectado!");
      delay(2000);
      lcd.clear();
      Serial.println("Nova tag (diferente da anterior)");
      lastTagID = tagID;

      tagName = "/USERTAG/" + tagID + "/Name";
      tagTemp = "/USERTAG/" + tagID + "/AIR/temp";
      tagIsOn = "/USERTAG/" + tagID + "/AIR/isOn";
      tagLed1 = "/USERTAG/" + tagID + "/LEDS/LED1";
      tagLed2 = "/USERTAG/" + tagID + "/LEDS/LED2";
      tagLed3 = "/USERTAG/" + tagID + "/LEDS/LED3";

      if (Firebase.ready() && signupOK) {
        // Send data only if a card is present

        Serial.print("Tag ID: ");
        Serial.println(tagID);
        sendDataPrevMillis = millis();
        if (Firebase.RTDB.getString(&fbdo, tagName)) {
          if (fbdo.dataType() == "string") {
            stringName = fbdo.stringData();
            lcd.setCursor(0, 0);
            lcd.print(stringName);
            client.publish("danielnobre09/feeds/Nome", stringName.c_str());
          }
        } else {
          Serial.println(fbdo.errorReason());
        }

        if (Firebase.RTDB.getBool(&fbdo, tagIsOn)) {
          if (fbdo.dataType() == "boolean") {
            boolIsOn = fbdo.boolData();
            if (boolIsOn) {
              if (Firebase.RTDB.getInt(&fbdo, tagTemp)) {
                if (fbdo.dataType() == "int") {
                  intTemp = fbdo.intData();
                  lcd.setCursor(0, 1);
                  lcd.print("T:");
                  lcd.print(intTemp);
                }
              } else {
                Serial.println(fbdo.errorReason());
              }
            }
          }
        } else {
          Serial.println(fbdo.errorReason());
        }

        if (Firebase.RTDB.getBool(&fbdo, tagLed1)) {
          if (fbdo.dataType() == "boolean") {
            boolLed1 = fbdo.boolData();
            if (boolLed1) {
              digitalWrite(LED1, HIGH);
            } else {
              digitalWrite(LED1, LOW);
            }
          }
        } else {
          Serial.println(fbdo.errorReason());
        }

        if (Firebase.RTDB.getBool(&fbdo, tagLed2)) {
          if (fbdo.dataType() == "boolean") {
            boolLed2 = fbdo.boolData();
            if (boolLed2) {
              digitalWrite(LED2, HIGH);
            } else {
              digitalWrite(LED2, LOW);
            }
          }
        } else {
          Serial.println(fbdo.errorReason());
        }

        if (Firebase.RTDB.getBool(&fbdo, tagLed3)) {
          if (fbdo.dataType() == "boolean") {
            boolLed3 = fbdo.boolData();
            if (boolLed3) {
              digitalWrite(LED3, HIGH);
            } else {
              digitalWrite(LED3, LOW);
            }
          }
        } else {
          Serial.println(fbdo.errorReason());
        }

        sendDataPrevMillis = millis();
      }
    } else {
      lcd.clear();
      Serial.println("Tag igual a anterior(parar)");
      lcd.print("Desconectado!");
      delay(2000);
      lastTagID.clear();
     // Same tag as the previous one, stop sending data and clear LCD
      isCardPresent = false;
      digitalWrite(LED1, LOW);
      digitalWrite(LED2, LOW);
      digitalWrite(LED3, LOW);
      lcd.clear();
    }
  }
  if (isCardPresent) {
        delay(6000);
        float umidade = dht.readHumidity();
        String umidadeStr = String(umidade);
        client.publish("danielnobre09/feeds/Umidade", umidadeStr.c_str());
        float temperatura = dht.readTemperature();
        String temperaturaStr = String(temperatura);
        client.publish("danielnobre09/feeds/Temperatura", temperaturaStr.c_str());
  }
}