#ifndef CONSTANTS_H
#define CONSTANTS_H

// Comunicacion de la terminal o Begin serial 
#define SERIAL_BAUDRATE 115200

// Suscrption
#define SUSCRIPTION_DESCRIPTION "notify_robert_iot_sensors_2025"
#define SUSCRIPTION_TYPE "temperature_humidity_and_gps_sensor"


// Receiver metadata
#define MODEL "ttgo-t-beam"
#define PROJECT "iot-2025-02"
#define ID "sensor12342"
#define TYPE "receiver"


// Configuracion lora
#define RADIO_CS_PIN 18
#define RADIO_RST_PIN 23
#define RADIO_DIO0_PIN 26
#define FREQUENCY 915E6

// Configuración para TTGO T-Beam
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS 18

// Configuracion del WIFI
#define SSID "joham"
#define PASSWORD "joham1015"

// Server variables
#define URL_SERVER "http://34.235.124.18:1026/v2"
#define URL_SERVER_NOTIFICATION_SUBS "http://quantumleap:8668/v2/notify"

// LED interno del TTGO T-Beam
#define LED 13


#endif // CONSTANTS_H
