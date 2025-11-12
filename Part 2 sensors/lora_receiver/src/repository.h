#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <LoRa.h>
#include "constants.h"

//JsonDocument Submit_an_incident(const Incident &incident)
DynamicJsonDocument Get_subscriptions();

bool Patch_entity_attrs(const char *entityId, const DynamicJsonDocument &bodyDoc);

bool Post_subscription(const char *description, const char *type, const char *notification_url);
bool Post_new_entity(const DynamicJsonDocument &bodyDoc);


#endif // REPOSITORY_H