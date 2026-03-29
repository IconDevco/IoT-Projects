// mqtt_client.cpp
#include "mqtt_client.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

static WiFiClient espClient;
static PubSubClient mqtt(espClient);

static SystemConfig *GLOBAL_CONFIG = nullptr;

// ------------------------------------------------------------
// MQTT CALLBACK (config updates)
// ------------------------------------------------------------
static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }

    Serial.printf("MQTT message on %s: %s\n", topic, msg.c_str());

    // Only handle config updates for now
    if (String(topic).endsWith("/config/update")) {
        File f = LittleFS.open("/config.json", "w");
        if (!f) {
            Serial.println("Failed to open config.json for writing");
            return;
        }
        f.print(msg);
        f.close();

        Serial.println("Config updated. Rebooting...");
        delay(500);
        ESP.restart();
    }
}

// ------------------------------------------------------------
// MQTT INIT
// ------------------------------------------------------------
bool mqttInit(const SystemConfig &config) {
    GLOBAL_CONFIG = (SystemConfig*)&config;

    mqtt.setServer(config.mqtt.broker.c_str(), config.mqtt.port);
    mqtt.setCallback(mqttCallback);

    Serial.println("Connecting to MQTT...");

    int attempts = 0;
    while (!mqtt.connected() && attempts < 10) {
        Serial.print("MQTT connect attempt... ");

        if (mqtt.connect(config.mqtt.client_id.c_str(),
                         config.mqtt.username.c_str(),
                         config.mqtt.password.c_str())) {
            Serial.println("connected.");
            break;
        }

        Serial.println("failed.");
        attempts++;
        delay(1000);
    }

    if (!mqtt.connected()) {
        Serial.println("MQTT connection failed.");
        return false;
    }

    // Subscribe to config updates
    String topic = config.mqtt.topic_prefix + "/config/update";
    mqtt.subscribe(topic.c_str());

    return true;
}

// ------------------------------------------------------------
// HEARTBEAT
// ------------------------------------------------------------
void mqttPublishHeartbeat() {
    if (!mqtt.connected()) return;

    StaticJsonDocument<128> doc;
    doc["timestamp"] = time(nullptr);

    String out;
    serializeJson(doc, out);

    mqtt.publish("hydro/system/heartbeat", out.c_str());
}

// ------------------------------------------------------------
// SUNRISE PACKET
// ------------------------------------------------------------
void mqttPublishSunrise(const SystemConfig &config, const ClimateData &climate) {
    if (!mqtt.connected()) return;

    StaticJsonDocument<512> doc;

    doc["sunrise"] = SUN.sunrise;
    doc["sunset"]  = SUN.sunset;
    doc["temp_f"]  = climate.temp_f;
    doc["humidity"] = climate.humidity;
    doc["uv_index"] = climate.uv_index;
    doc["cloud_cover"] = climate.cloud_cover;

    String out;
    serializeJson(doc, out);

    String topic = config.mqtt.topic_prefix + "/day/sunrise";
    mqtt.publish(topic.c_str(), out.c_str());
}

// ------------------------------------------------------------
// SUNSET PACKET
// ------------------------------------------------------------
void mqttPublishSunset(const SystemConfig &config) {
    if (!mqtt.connected()) return;

    StaticJsonDocument<512> doc;

    JsonArray arr = doc.createNestedArray("pumps");

    for (auto &p : config.pumps) {
        JsonObject o = arr.createNestedObject();
        o["id"] = p.id;
        o["run_minutes"] = p.run_minutes;
    }

    String out;
    serializeJson(doc, out);

    String topic = config.mqtt.topic_prefix + "/day/sunset";
    mqtt.publish(topic.c_str(), out.c_str());
}

// ------------------------------------------------------------
// PROCESS CONFIG UPDATES
// ------------------------------------------------------------
void mqttProcessUpdates(SystemConfig &config) {
    if (mqtt.connected()) {
        mqtt.loop();
    }
}