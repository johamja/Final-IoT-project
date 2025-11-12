#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "repository.h"
#include "config.h"


void Http_errors(int httpCode) {
    // Client error (4xx)
    if (httpCode >= 400 && httpCode < 500) {
        Serial.printf("[HTTP] [Get_subscriptions] Client error: %d\n", httpCode);
    }
    // Server error (5xx)
    else if (httpCode >= 500) {
        Serial.printf("[HTTP] [Get_subscriptions] Server error: %d\n", httpCode);
    }
    // Otros códigos inesperados
    else {
        Serial.printf("[HTTP] [Get_subscriptions] Unexpected HTTP code: %d\n", httpCode);
    }
}

DynamicJsonDocument Get_subscriptions() {
    HTTPClient http;
    const String url = URL_SERVER  "/subscriptions";
    Serial.println("\n[HTTP] [Get_subscriptions] begin... " + url);

    http.setReuse(true);
    http.begin(url);
    http.setTimeout(60000);
    http.addHeader("Content-Type", "application/json");

    const int httpCode = http.GET();

    // No connection / network error
    if (httpCode <= 0) {
        Serial.printf("[HTTP] [Get_subscriptions] GET failed, error: %s\n",
                      HTTPClient::errorToString(httpCode).c_str());
        http.end();
        return DynamicJsonDocument(1); // vacío
    }

    // OK -> parse JSON
    if (httpCode == HTTP_CODE_OK) {
        const String response = http.getString();
        Serial.println("[HTTP] [Get_subscriptions] Response received:");
        Serial.println(response);

        DynamicJsonDocument responseDoc(4096);
        DeserializationError error = deserializeJson(responseDoc, response);

        if (!error) {
            http.end();
            return responseDoc;
        }

        Serial.print("[HTTP] [Get_subscriptions] Error parsing JSON: ");
        Serial.println(error.c_str());
        http.end();
        return DynamicJsonDocument(1); // vacío
    }

    Http_errors(httpCode);

    http.end();
    return DynamicJsonDocument(1); // vacío
}

// Crea y publica una suscripción con el JSON especificado.
// Retorna true si el POST fue exitoso (HTTP 200/201), false en caso contrario.
bool Post_subscription(const char *description, const char *type, const char *notification_url)
{
    HTTPClient http;
    const String url = URL_SERVER "/subscriptions";
    Serial.println("\n[HTTP] [Post_subscription] begin... " + url);

    http.setReuse(true);
    http.begin(url);
    http.setTimeout(60000);
    http.addHeader("Content-Type", "application/json");

    // Construir payload JSON
    DynamicJsonDocument payload(2048);

    payload["description"] = description;

    // subject
    JsonObject subject = payload.createNestedObject("subject");
    JsonArray entities = subject.createNestedArray("entities");
    JsonObject ent0 = entities.createNestedObject();
    ent0["idPattern"] = ".*";
    ent0["type"] = type;

    JsonObject condition = subject.createNestedObject("condition");
    JsonArray condAttrs = condition.createNestedArray("attrs");
    const char *cond_list[] = {
        "model", "project", "latitude", "longitude",
        "temperature", "humidity", "lora_received_power",
        "lora_signal_quality", "lora_frequency_error", "timestamp_received"
    };
    for (auto &s : cond_list) condAttrs.add(s);

    // notification
    JsonObject notification = payload.createNestedObject("notification");
    JsonArray notifAttrs = notification.createNestedArray("attrs");
    const char *notif_list[] = {
        "id", "model", "project", "latitude", "longitude",
        "temperature", "humidity", "lora_received_power",
        "lora_signal_quality", "lora_frequency_error", "timestamp_received"
    };
    for (auto &s : notif_list) notifAttrs.add(s);

    JsonObject httpObj = notification.createNestedObject("http");
    httpObj["url"] = notification_url;

    JsonArray metadata = notification.createNestedArray("metadata");
    metadata.add("dateCreated");
    metadata.add("dateModified");

    // Serializar
    String body;
    serializeJson(payload, body);
    Serial.println("[HTTP] [Post_subscription] Payload:");
    Serial.println(body);

    int httpCode = http.POST(body);

    if (httpCode <= 0) {
        Serial.printf("[HTTP] [Post_subscription] POST failed, error: %s\n",
                      HTTPClient::errorToString(httpCode).c_str());
        http.end();
        return false;
    }

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        Serial.printf("[HTTP] [Post_subscription] Success, code: %d\n", httpCode);
        String response = http.getString();
        if (response.length()) {
            Serial.println("[HTTP] [Post_subscription] Response:");
            Serial.println(response);
        }
        http.end();
        return true;
    }

    // Otros errores
    Http_errors(httpCode);
    String response = http.getString();
    if (response.length()) {
        Serial.println("[HTTP] [Post_subscription] Response:");
        Serial.println(response);
    }
    http.end();
    return false;
}

// PATCH attributes to an entity.
// entityId: id of the entity (e.g. "sensor20")
// bodyDoc: DynamicJsonDocument containing the attributes to patch
// Returns true on success (204/200/201), false on any error.
bool Patch_entity_attrs(const char *entityId, const DynamicJsonDocument &bodyDoc)
{
    HTTPClient http;
    String url = String(URL_SERVER) + "/entities/" + String(entityId) + "/attrs";
    Serial.println("\n[HTTP] [Patch_entity_attrs] begin... " + url);

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // Serializar documento al cuerpo
    String body;
    serializeJson(bodyDoc, body);
    Serial.println("[HTTP] [Patch_entity_attrs] Body:");
    Serial.println(body);

    // Enviar PATCH. Algunos cores soportan http.PATCH(body) directamente;
    // usamos sendRequest para mayor compatibilidad.
    int httpCode = http.sendRequest("PATCH", (uint8_t *)body.c_str(), body.length());

    delay(2000);
    http.end();

    // Error de conexión / red
    if (httpCode <= 0)
    {
        Serial.printf("[HTTP] [Patch_entity_attrs] PATCH failed, error: %s\n",
                      HTTPClient::errorToString(httpCode).c_str());
        http.end();
        return false;
    }

    // Aceptar 204 No Content (typical FIWARE), 200 o 201 como éxito
    if (httpCode == HTTP_CODE_NO_CONTENT || httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED)
    {
        Serial.printf("[HTTP] [Patch_entity_attrs] Success, code: %d\n", httpCode);
        /*
        String response = http.getString();
        Serial.println(response);
        */
        http.end();
        return true;
    }

    // Otros códigos -> registrar y devolver false
    Http_errors(httpCode);
    String response = http.getString();
    if (response.length())
    {
        Serial.println("[HTTP] [Patch_entity_attrs] Response:");
        Serial.println(response);
    }

    http.end();
    return false;
}


// POST new entity to Orion Context Broker.
// Recibe solo el JSON del entity y retorna true si la respuesta es 201 Created.
bool Post_new_entity(const DynamicJsonDocument &bodyDoc)
{
    HTTPClient http;
    const String url = String(URL_SERVER) + "/entities";

    http.setReuse(true);
    http.begin(url);
    http.setTimeout(60000);
    http.addHeader("Content-Type", "application/json");

    String body;
    serializeJson(bodyDoc, body);

    int httpCode = http.POST(body);

    if (httpCode == HTTP_CODE_CREATED) { // 201
        http.end();
        return true;
    }

    http.end();
    return false;
}

