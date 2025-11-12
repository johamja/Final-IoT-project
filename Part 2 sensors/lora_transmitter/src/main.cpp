#include <Arduino.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include "ClosedCube_HDC1080.h"
#include "LoRaBoards.h"
#include "constants.h"

// ----- CONFIGURACIÓN LORA -----
#ifndef CONFIG_RADIO_FREQ
#define CONFIG_RADIO_FREQ 915.0
#endif
#ifndef CONFIG_RADIO_OUTPUT_POWER
#define CONFIG_RADIO_OUTPUT_POWER 17
#endif
#ifndef CONFIG_RADIO_BW
#define CONFIG_RADIO_BW 125.0
#endif

// ----- CONFIGURACIÓN GPS -----
TinyGPSPlus gps;
HardwareSerial gpsSerial(1); // UART1 para el GPS
#define GPS_RX_PIN 34
#define GPS_TX_PIN 12
#define GPS_BAUD 9600

// ----- CONFIGURACIÓN HDC1080 -----
ClosedCube_HDC1080 hdc1080;
// Use board-default I2C pins from utilities.h (I2C_SDA / I2C_SCL)

// ----- VARIABLES -----
unsigned long lastSend = 0;
int counter = 0;

// -------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando TTGO T-Beam LoRa + GPS + HDC1080");

  // --- Enciende GPS (AXP2101 puede apagarlo por defecto) ---
  pinMode(14, OUTPUT); // algunas T-Beam V1.2 usan GPIO14 para power GPS
  digitalWrite(14, HIGH);
  delay(500);

  // --- Inicializa GPS ---
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("GPS inicializado");

  // --- Inicializa sensor HDC1080 ---
#ifdef I2C_SDA
  Wire.begin(I2C_SDA, I2C_SCL);
#else
  // Fallback: let Wire use default pins for the board
  Wire.begin();
#endif
  hdc1080.begin(0x40);
  Serial.print("Fabricante HDC1080 ID: ");
  Serial.println(hdc1080.readManufacturerId(), HEX);
  Serial.print("Dispositivo HDC1080 ID: ");
  Serial.println(hdc1080.readDeviceId(), HEX);

  // --- Inicializa LoRa ---
  setupBoards();
  delay(1500);

#ifdef RADIO_TCXO_ENABLE
  pinMode(RADIO_TCXO_ENABLE, OUTPUT);
  digitalWrite(RADIO_TCXO_ENABLE, HIGH);
#endif

  LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
  if (!LoRa.begin(CONFIG_RADIO_FREQ * 1000000))
  {
    Serial.println("Error al iniciar LoRa!");
    while (1)
      ;
  }
  LoRa.setTxPower(CONFIG_RADIO_OUTPUT_POWER);
  LoRa.setSignalBandwidth(CONFIG_RADIO_BW * 1000);
  LoRa.setSpreadingFactor(10);
  LoRa.setCodingRate4(7);
  LoRa.setSyncWord(0xAB);

  Serial.println("LoRa, GPS y HDC1080 listos!");
}

// -------------------------------------------------------------------
bool leerGPS(float &lat, float &lng)
{
  while (gpsSerial.available() > 0)
  {
    gps.encode(gpsSerial.read());
  }

  if (gps.location.isUpdated() && gps.location.isValid())
  {
    lat = gps.location.lat();
    lng = gps.location.lng();
    return true;
  }
  return false;
}

// -------------------------------------------------------------------
void loop()
{
  float lat = 0, lng = 0;
  double temp = hdc1080.readTemperature();
  double hum = hdc1080.readHumidity();

  if (millis() - lastSend > 3000)
  { // cada 1 segundo
    lastSend = millis();

    if (leerGPS(lat, lng))
    {
      Serial.printf("GPS: %.6f, %.6f | Temp: %.2f °C | Hum: %.2f %%\n", lat, lng, temp, hum);

      // Construir JSON manualmente (sin librerías) con metadatos según el formato solicitado
      // Aumentamos el buffer para tener margen suficiente
      char packetBuf[512]; // ajustar si necesita más campos
      int written = snprintf(packetBuf, sizeof(packetBuf),
        "{"
          "\"latitude\":{"
            "\"value\":%.6f,\"type\":\"Float\""
          "},"
          "\"longitude\":{"
            "\"value\":%.6f,\"type\":\"Float\""
          "},"
          "\"temperature\":{"
            "\"value\":%.2f,\"type\":\"Float\""
          "},"
          "\"humidity\":{"
            "\"value\":%.2f,\"type\":\"Float\""
          "}"
        "}",
        lat, lng, temp, hum);

      // Verificar overflow
      if (written < 0 || written >= (int)sizeof(packetBuf))
      {
        // manejar error (por ejemplo truncar o no enviar)
        Serial.println("Packet JSON too large, not sent");
      }
      else
      {
        // Enviar por LoRa
        LoRa.beginPacket();
        LoRa.print(packetBuf);
        LoRa.endPacket();
        Serial.printf("Enviado por LoRa: #%d\n", counter);
      }
      
      counter++;
    }
    else
    {
      Serial.println("Esperando señal GPS...");
    }
  }
}
