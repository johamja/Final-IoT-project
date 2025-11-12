#include <LoRa.h>
#include "constants.h"

void setup() {
    Serial.begin(SERIAL_BAUDRATE);
    pinMode(LED, OUTPUT);
    
    Serial.println("TTGO T-Beam LoRa Receiver");
    Serial.println("Frecuencia: 915 MHz");
    
    // Configuración de pines LoRa para TTGO T-Beam
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    
    // Inicializar LoRa en 915 MHz
    if (!LoRa.begin(915E6)) {
        Serial.println("Error iniciando LoRa!");
        while (1);
    }
    
    Serial.println("LoRa Receptor listo!");
    Serial.println("Esperando datos...");
    Serial.println();
}

void loop() {
    // Verificar si hay datos recibidos
    int packetSize = LoRa.parsePacket();
    
    if (packetSize) {
        // Encender LED cuando llega dato
        digitalWrite(LED, HIGH);
        
        // Leer el mensaje
        String mensaje = "";
        while (LoRa.available()) {
            mensaje += (char)LoRa.read();
        }
        
        // Mostrar información simple en Serial
        Serial.print("📡 Dato recibido: ");
        Serial.println(mensaje);
        Serial.print("📶 RSSI: ");
        Serial.print(LoRa.packetRssi());
        Serial.println(" dBm");
        Serial.println("-------------------");
        
        // Apagar LED después de 100ms
        delay(100);
        digitalWrite(LED, LOW);
    }
    
    // Pequeña pausa
    delay(50);
}