#include "MqttManager.h"

#include <Logger.h>
#include <cstring>
#include "esp_err.h"

// The MQTT event task: connection lifecycle + inbound routing. Everything here
// runs on esp-mqtt's own task, so it only logs, publishes and queues.

namespace {

const char* refusedReason(esp_mqtt_connect_return_code_t code) {
    switch (code) {
        case MQTT_CONNECTION_REFUSE_BAD_USERNAME:
        case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED:
            return "username or password rejected";
        case MQTT_CONNECTION_REFUSE_ID_REJECTED:
            return "client ID rejected";
        case MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE:
            return "broker unavailable";
        case MQTT_CONNECTION_REFUSE_PROTOCOL:
            return "protocol version not supported";
        default:
            return "refused";
    }
}

}  // namespace

const char* MqttManager::userForLog() const {
    return cfg_.user.length() > 0 ? cfg_.user.c_str() : "(no username)";
}

void MqttManager::eventHandler(void* args, esp_event_base_t /*base*/, int32_t id, void* data) {
    auto* self = static_cast<MqttManager*>(args);
    auto* ev = static_cast<esp_mqtt_event_handle_t>(data);
    switch (static_cast<esp_mqtt_event_id_t>(id)) {
        case MQTT_EVENT_CONNECTED:
            self->connected_ = true;
            self->onConnected();
            break;
        case MQTT_EVENT_DISCONNECTED:
            self->onDisconnected();
            break;
        case MQTT_EVENT_ERROR:
            self->onError(ev->error_handle);
            break;
        case MQTT_EVENT_DATA:
            // Small single-chunk payloads only; ignore fragmented continuations.
            if (ev->topic_len > 0 && ev->current_data_offset == 0) {
                self->enqueueInbound(ev->topic, ev->topic_len, ev->data, ev->data_len);
            }
            break;
        default:
            break;
    }
}

void MqttManager::onConnected() {
    lastFailureKey_ = 0;
    Logger.info("MQTT: connected to %s:%u as %s", cfg_.host.c_str(),
                static_cast<unsigned>(cfg_.port), userForLog());
    publishDiscovery();
    publishAvailability(true);
    esp_mqtt_client_subscribe(client_, topicLightSet_.c_str(), 1);
    esp_mqtt_client_subscribe(client_, topicVuSet_.c_str(), 1);
    esp_mqtt_client_subscribe(client_, topicSleepSet_.c_str(), 1);
    // Defer the initial full state publish to update() on the main loop so the
    // change-detection cache is only ever mutated from one task. (onConnected runs
    // on the MQTT event task; discovery/availability/subscribe above don't touch it.)
    forcePublish_ = true;
}

void MqttManager::onDisconnected() {
    // A failed attempt also ends in DISCONNECTED; only a live session dropping is news.
    if (connected_) {
        Logger.warning("MQTT: lost connection to %s:%u, reconnecting", cfg_.host.c_str(),
                       static_cast<unsigned>(cfg_.port));
    }
    connected_ = false;
}

// esp-mqtt retries every 10 s, so a bad password would otherwise log forever.
// Each distinct failure is logged once; a success or a different failure re-arms it.
void MqttManager::onError(const esp_mqtt_error_codes_t* err) {
    if (!err) return;

    if (err->error_type == MQTT_ERROR_TYPE_SUBSCRIBE_FAILED) {
        Logger.warning("MQTT: broker refused a subscription");
        return;
    }

    const bool refused = err->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED;
    const int detail = refused ? static_cast<int>(err->connect_return_code)
                     : err->esp_transport_sock_errno ? err->esp_transport_sock_errno
                                                     : static_cast<int>(err->esp_tls_last_esp_err);
    const int key = (static_cast<int>(err->error_type) << 24) ^ detail;
    if (key == lastFailureKey_) return;
    lastFailureKey_ = key;

    const unsigned port = static_cast<unsigned>(cfg_.port);
    if (refused) {
        Logger.warning("MQTT: %s:%u refused the connection as %s: %s (retrying every 10 s)",
                       cfg_.host.c_str(), port, userForLog(),
                       refusedReason(err->connect_return_code));
    } else if (err->esp_transport_sock_errno) {
        Logger.warning("MQTT: cannot reach %s:%u: %s (retrying every 10 s)",
                       cfg_.host.c_str(), port, strerror(err->esp_transport_sock_errno));
    } else if (err->esp_tls_last_esp_err != ESP_OK) {
        Logger.warning("MQTT: cannot reach %s:%u: %s (retrying every 10 s)",
                       cfg_.host.c_str(), port, esp_err_to_name(err->esp_tls_last_esp_err));
    } else {
        Logger.warning("MQTT: connection to %s:%u failed (retrying every 10 s)",
                       cfg_.host.c_str(), port);
    }
}
