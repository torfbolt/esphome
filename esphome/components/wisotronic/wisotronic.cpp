#include "wisotronic.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wisotronic {

static const char *const TAG = "wisotronic";

WisotronicCustomComponent::WisotronicCustomComponent(
    uart::UARTComponent *parent): UARTDevice(parent) {}

void WisotronicCustomComponent::setup() {}

void WisotronicCustomComponent::loop() {
    while (available()) {
        uint8_t byte = read();
        stream_to_packet(byte);
    }
}

void WisotronicCustomComponent::dump_config() {
    // Sensors
    LOG_SENSOR("", "Control Unit Temperature", this->control_unit_temperature_sensor_);
    LOG_SENSOR("", "Outdoor Temperature", this->outdoor_temperature_sensor_);
    LOG_SENSOR("", "Indoor Temperature 1", this->indoor_temperature_sensor_);
    LOG_SENSOR("", "Indoor Humidity", this->indoor_humidity_sensor_);
    LOG_SENSOR("", "Wind Speed", this->wind_speed_sensor_);
    LOG_SENSOR("", "Rain", this->rain_sensor_);
    LOG_SENSOR("", "Photo1 Brightness", this->photo1_brightness_sensor_);
    LOG_SENSOR("", "Photo2 Brightness", this->photo2_brightness_sensor_);
    LOG_SENSOR("", "Photo3 Brightness", this->photo3_brightness_sensor_);
    LOG_SENSOR("", "Twilight Brightness", this->twilight_brightness_sensor_);
    // Covers
    if (awning_cover_ != nullptr)
        ESP_LOGCONFIG(TAG, "Awning Cover: %p", awning_cover_);
    if (window_cover_ != nullptr)
        ESP_LOGCONFIG(TAG, "Window Cover: %p", window_cover_);
    if (blind1_cover_ != nullptr)
        ESP_LOGCONFIG(TAG, "Blind 1 Cover: %p", blind1_cover_);
    if (blind2_cover_ != nullptr)
        ESP_LOGCONFIG(TAG, "Blind 2 Cover: %p", blind2_cover_);
}

void WisotronicCustomComponent::stream_to_packet(uint8_t byte) {
    buffer_[p_buffer_++] = byte;
    if (p_buffer_ >= sizeof(buffer_)) p_buffer_ = 0;
    if (p_buffer_ >= 3) {
        if ((buffer_[p_buffer_ - 2] == 0x23) && ((buffer_[p_buffer_ - 1] == 0x00) 
                || (buffer_[p_buffer_ - 1] == 0xfd))) {
            uint8_t sum = 0;
            for (int i = 0; i < p_buffer_ - 3; i++) sum += buffer_[i];
            if (sum == buffer_[p_buffer_ - 3]) {
                parse_wisotronic_packet(p_buffer_ - 2);
            } else {
                ESP_LOGD(TAG, "Found packet with checksum mismatch, %d != %d", 
                    sum, buffer_[p_buffer_ - 3]);
            }
            buffer_[0] = buffer_[p_buffer_ - 2];
            buffer_[1] = buffer_[p_buffer_ - 1];
            p_buffer_ = 2;
        }
    }
}

void WisotronicCustomComponent::parse_wisotronic_packet(uint8_t len) {
    if (len == 22 && buffer_[1] == 0x00) {
        if (control_unit_temperature_sensor_ != nullptr)
            control_unit_temperature_sensor_->publish_state(buffer_[11] / 2.0 - 25);
    } else if (len == 46) {
        if (awning_cover_ != nullptr) {
            awning_cover_->position = 1.0f - buffer_[13] / 200.0f;
            awning_cover_->publish_state();
        }
        if (window_cover_ != nullptr) {
            window_cover_->position = buffer_[21] / 200.0f;
            window_cover_->publish_state();
        }
        if (blind1_cover_ != nullptr) {
            blind1_cover_->position = 1.0f - buffer_[29] / 200.0f;
            blind1_cover_->tilt = 0.5f - int8_t(buffer_[30]) / 160.0f;
            blind1_cover_->publish_state();
        }
        if (blind2_cover_ != nullptr) {
            blind2_cover_->position = 1.0f - buffer_[31] / 200.0f;
            blind2_cover_->tilt = 0.5f - int8_t(buffer_[32]) / 160.0f;
            blind2_cover_->publish_state();
        }
        // wind_sensor updates
        if (wind_speed_sensor_ != nullptr) wind_speed_sensor_->publish_state(buffer_[35] / 5.0);
    } else if (len == 58) {
        if (rain_sensor_ != nullptr)
            rain_sensor_->publish_state(buffer_[11]);
        if (indoor_temperature_sensor_ != nullptr)
            indoor_temperature_sensor_->publish_state(buffer_[14] / 2.0 - 25);
        if (outdoor_temperature_sensor_ != nullptr)
            outdoor_temperature_sensor_->publish_state(buffer_[23] / 2.0 - 25);
        if (indoor_humidity_sensor_ != nullptr)
            indoor_humidity_sensor_->publish_state(buffer_[26] / 2.0);
        if (photo1_brightness_sensor_ != nullptr)
            photo1_brightness_sensor_->publish_state(buffer_[29] / 2.0 * 1e3);
        if (photo2_brightness_sensor_ != nullptr)
            photo2_brightness_sensor_->publish_state(buffer_[32] / 2.0 * 1e3);
        if (photo3_brightness_sensor_ != nullptr)
            photo3_brightness_sensor_->publish_state(buffer_[35] / 2.0 * 1e3);
        if (twilight_brightness_sensor_ != nullptr)
            twilight_brightness_sensor_->publish_state(buffer_[38] * 2.0);
    }
}

cover::CoverTraits WisotronicCover::get_traits() {
    auto traits = cover::CoverTraits();
    traits.set_is_assumed_state(false);
    traits.set_supports_position(true);
    traits.set_supports_tilt(true);
    return traits;
}

void WisotronicCover::control(const cover::CoverCall &call) {
    // not implemented
}

}  // namespace wisotronic
}  // namespace esphome
