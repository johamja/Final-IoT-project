#include <Arduino.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "constants.h"
#include "services.h"
#include "repository.h"

int state = 1;

// Buffer JSON persistente entre iteraciones
DynamicJsonDocument orion_data_new(8192);

void setup()
{
    Serial.begin(SERIAL_BAUDRATE);
    pinMode(LED, OUTPUT);

    Lora_connection();
    WiFi_connection();

    // Check if the subscription is active
    while (true)
    {
        // Reintentar WiFi si se cayó
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("[INFO] WiFi desconectado - Reintentando...");
            WiFi_connection();
            delay(1000);
            continue;
        }
        
        DynamicJsonDocument sucriptions(4096);
        sucriptions = Get_subscriptions();
        delay(2000);
        if (sucriptions.is<JsonArray>() || sucriptions.is<z>())
        {
            if (Has_description_and_type(sucriptions, SUSCRIPTION_DESCRIPTION, SUSCRIPTION_TYPE))
            {
                Serial.println("[GOOD] Description and type found");
                delay(2000);
                break;
            }
            else
            {
                Serial.println("[BAD] The condition (description+type) was not met");
                bool subscription = Post_subscription(SUSCRIPTION_DESCRIPTION, SUSCRIPTION_TYPE, URL_SERVER_NOTIFICATION_SUBS);
                delay(2000);
                if (subscription)
                {
                    Serial.println("[GOOD] The subscription was created");
                    break;
                }
            }
        }
        else
        {
            Serial.println("[BAD] Unable to get subscriptions, sleeping for 3 seconds");
            // sleep(3000);  // En ESP32 esto es en segundos; usa delay en ms
            delay(3000);
        }
    }

    Serial.println("[OK] FIWARE LoRa Receiver Ready - Waiting for data...");
}

void loop()
{

    switch (state)
    {
    case 1: // RECEIVE - CHIRP - Esperar datos LoRa del receptor
    {
        int packetSize = LoRa.parsePacket();

        if (packetSize)
        {
            // Read message
            String message = "";
            while (LoRa.available())
            {
                message += (char)LoRa.read();
            }

            int rssi = LoRa.packetRssi();
            long error_hz = LoRa.packetFrequencyError();
            float snr = LoRa.packetSnr();

            // ...existing code...
            Serial.println("=== DATO RECIBIDO ===");
            Serial.printf("Mensaje: %s\n", message.c_str());
            Serial.printf("RSSI: %d dBm\t Packet Frequency Error: %ld Hz\t SNR: %.2f dB\n",
                          rssi, error_hz, snr);

            // Create JSON
            orion_data_new = Create_orion_package(message, rssi, snr, error_hz);
            String preview;
            serializeJson(orion_data_new, preview);
            Serial.println("[XXXXXXXXXXXXXXXXXXXXXXXXXXXXX] Payload :");
            Serial.println(preview);
            // ...existing code...
            state = 2;
        }

        break;
    }

    case 2:
    {

        Serial.println("[INFO] Sending data to FIWARE Orion...");

        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("WiFi desconectado - Reconectando...");
            WiFi_connection();
            delay(1000);
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            bool success = Patch_entity_attrs(ID, orion_data_new);
            if (success)
            {
                Serial.println("[GOOD] Data successfully sent to FIWARE");
                state = 3;
            }
            else
            {
                Serial.println("[BAD] Failed to send to FIWARE");
                Serial.println("[INFO] Create the entity in Orion");

                DynamicJsonDocument orion_data_copy(8192);

                orion_data_copy = orion_data_new;

                // Campos base de la entidad
                orion_data_copy["id"] = ID;     // e.g. "sensor2"
                orion_data_copy["type"] = SUSCRIPTION_TYPE; // e.g. "receiver"
                success = Post_new_entity(orion_data_copy);

                if (success)
                {
                    Serial.println("[GOOD] Data successfully sent to FIWARE and a new entity was created");
                    state = 3;
                }
                else
                {
                    Serial.println("[BAD] Unable to create entity in Orion");
                    delay(1000);
                }
            }
        }
        else
        {
            Serial.println("[BAD] Could not connect to WiFi");
        }

        break;
    }

    case 3: // WAIT - Breve espera
    {
        Serial.println("[OK] Waiting 2 seconds...");
        delay(2000);
        state = 1;
        break;
    }

    default:
    {
        Serial.println("[BAD] Unrecognized State - Restarting");
        state = 1;
        break;
    }
    }
}