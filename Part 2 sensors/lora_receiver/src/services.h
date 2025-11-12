#ifndef services
#define services

void Lora_connection();
void WiFi_connection();
DynamicJsonDocument Create_orion_package(String message, int rssi, float snr, long error_hz);
bool Has_description_and_type(const DynamicJsonDocument &doc, const char *wanted_description, const char *wanted_type);

#endif