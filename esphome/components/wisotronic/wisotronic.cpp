#include "wisotronic.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace wisotronic {

ESPHOME_LOG_TAG(TAG, "wisotronic");

WisotronicCustomComponent::WisotronicCustomComponent(uart::UARTComponent *parent) : UARTDevice(parent) {}

void WisotronicCustomComponent::setup() {}

void WisotronicCustomComponent::loop() {
  while (available()) {
    uint8_t byte = read();
    stream_to_packet(byte);
  }
}

void WisotronicCustomComponent::write_frame(const uint8_t *frame, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    write(frame[i]);
  }
  if (this->debug_enabled_) {
    char hex[3 * 128 + 1];
    size_t pos = 0;
    for (uint8_t i = 0; i < len; i++) {
      pos += snprintf(hex + pos, sizeof(hex) - pos, "%02x ", frame[i]);
    }
    ESP_LOGD(TAG, "TX class=0x%02x len=%d: %s", frame[1], len, hex);
  }
}

void WisotronicCustomComponent::dump_config() {
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
  if (awning_cover_ != nullptr) {
    ESP_LOGCONFIG(TAG, "Awning Cover: %p", awning_cover_);
  }
  if (window_cover_ != nullptr) {
    ESP_LOGCONFIG(TAG, "Window Cover: %p", window_cover_);
  }
  if (blind1_cover_ != nullptr) {
    ESP_LOGCONFIG(TAG, "Blind 1 Cover: %p", blind1_cover_);
  }
  if (blind2_cover_ != nullptr) {
    ESP_LOGCONFIG(TAG, "Blind 2 Cover: %p", blind2_cover_);
  }
}

void WisotronicCustomComponent::stream_to_packet(uint8_t byte) {
  buffer_[p_buffer_++] = byte;
  if (p_buffer_ >= sizeof(buffer_))
    p_buffer_ = 0;

  if (p_buffer_ == 1 && buffer_[0] != 0x23) {
    p_buffer_ = 0;
    return;
  }

  if (p_buffer_ < 9)
    return;

  uint8_t L = buffer_[8];
  uint8_t total_len;

  // Poll frames are 11 bytes fixed length: 23 00 <addr> 10 01 <dataset> 00 00 00 00 CS
  // Data frames use L + 10 where L is the payload length
  if (buffer_[3] == 0x10 && buffer_[4] == 0x01 && L == 0)
    total_len = 11;
  else
    total_len = L + 10;

  if (total_len > sizeof(buffer_)) {
    p_buffer_ = 0;
    return;
  }

  if (p_buffer_ < total_len)
    return;

  uint8_t checksum = buffer_[total_len - 1];
  uint8_t sum = 0;
  for (int i = 0; i < total_len - 1; i++)
    sum += buffer_[i];

  if (sum != checksum) {
    ESP_LOGD(TAG, "Checksum mismatch: %d != %d", sum, checksum);
    // Resynchronize: search for next 0x23 start byte in remaining buffer
    int start = -1;
    for (int i = 1; i < p_buffer_; i++) {
      if (buffer_[i] == 0x23) {
        start = i;
        break;
      }
    }
    if (start < 0) {
      p_buffer_ = 0;
    } else {
      std::memmove(buffer_, buffer_ + start, p_buffer_ - start);
      p_buffer_ -= start;
    }
    return;
  }

  if (this->debug_enabled_) {
    char hex[3 * 128 + 1];
    size_t pos = 0;
    for (uint8_t i = 0; i < total_len; i++) {
      pos += snprintf(hex + pos, sizeof(hex) - pos, "%02x ", buffer_[i]);
    }
    ESP_LOGD(TAG, "RX class=0x%02x len=%d: %s", buffer_[1], total_len, hex);
  }
  parse_wisotronic_packet(total_len);

  int remaining = p_buffer_ - total_len;
  if (remaining > 0)
    std::memmove(buffer_, buffer_ + total_len, remaining);
  p_buffer_ = remaining;
}

void WisotronicCustomComponent::parse_wisotronic_packet(uint8_t len) {
  uint8_t L = buffer_[8];
  uint8_t class_byte = buffer_[1];
  uint8_t *payload = buffer_ + 9;
  uint8_t offset = 0;

  while (offset + 2 <= L) {
    uint8_t tag = payload[offset];
    uint8_t vlen = payload[offset + 1];
    if (offset + 2 + vlen > L)
      break;
    uint8_t *val = payload + offset + 2;

    process_tlv(class_byte, tag, vlen, val);
    offset += 2 + vlen;
  }
}

void WisotronicCustomComponent::process_tlv(uint8_t class_byte, uint8_t tag, uint8_t vlen, uint8_t *val) {
  if (class_byte == 0x00) {
    if (tag == 0x00 && vlen >= 1 && control_unit_temperature_sensor_ != nullptr) {
      control_unit_temperature_sensor_->publish_state(val[0] / 2.0f - 25.0f);
    }
  } else if (class_byte == 0xfd) {
    switch (tag) {
      case 0x00:  // awning block (len 6)
        if (vlen >= 3 && awning_cover_ != nullptr) {
          awning_cover_->position = 1.0f - val[2] / 200.0f;
          awning_cover_->publish_state();
        }
        break;
      case 0x01:  // window block (len 6)
        if (vlen >= 3 && window_cover_ != nullptr) {
          window_cover_->position = val[2] / 200.0f;
          window_cover_->publish_state();
        }
        break;
      case 0x02:  // blind block (len 6): pos1 tilt1 pos2 tilt2
        if (vlen >= 5) {
          if (blind1_cover_ != nullptr) {
            blind1_cover_->position = 1.0f - val[2] / 200.0f;
            blind1_cover_->tilt = 0.5f - static_cast<int8_t>(val[3]) / 160.0f;
            blind1_cover_->publish_state();
          }
          if (blind2_cover_ != nullptr) {
            blind2_cover_->position = 1.0f - val[4] / 200.0f;
            blind2_cover_->tilt = 0.5f - static_cast<int8_t>(val[5]) / 160.0f;
            blind2_cover_->publish_state();
          }
        }
        break;
      case 0x03:  // wind speed (len 1)
        if (vlen >= 1 && wind_speed_sensor_ != nullptr)
          wind_speed_sensor_->publish_state(val[0] / 5.0f);
        break;
      case 0x04:  // rain
        if (vlen >= 1 && rain_sensor_ != nullptr)
          rain_sensor_->publish_state(static_cast<float>(val[0]));
        break;
      case 0x05:  // indoor temperature
        if (vlen >= 1 && indoor_temperature_sensor_ != nullptr)
          indoor_temperature_sensor_->publish_state(val[0] / 2.0f - 25.0f);
        break;
      case 0x08:  // outdoor temperature
        if (vlen >= 1 && outdoor_temperature_sensor_ != nullptr)
          outdoor_temperature_sensor_->publish_state(val[0] / 2.0f - 25.0f);
        break;
      case 0x09:  // humidity
        if (vlen >= 1 && indoor_humidity_sensor_ != nullptr)
          indoor_humidity_sensor_->publish_state(val[0] / 2.0f);
        break;
      case 0x0a:  // photo1 brightness
        if (vlen >= 1 && photo1_brightness_sensor_ != nullptr)
          photo1_brightness_sensor_->publish_state(val[0] / 2.0f * 1e3f);
        break;
      case 0x0b:  // photo2 brightness
        if (vlen >= 1 && photo2_brightness_sensor_ != nullptr)
          photo2_brightness_sensor_->publish_state(val[0] / 2.0f * 1e3f);
        break;
      case 0x0c:  // photo3 brightness
        if (vlen >= 1 && photo3_brightness_sensor_ != nullptr)
          photo3_brightness_sensor_->publish_state(val[0] / 2.0f * 1e3f);
        break;
      case 0x0d:  // twilight brightness
        if (vlen >= 1 && twilight_brightness_sensor_ != nullptr)
          twilight_brightness_sensor_->publish_state(val[0] * 2.0f);
        break;
      case 0x12:  // blind 1 position (len 1)
        if (vlen >= 1 && blind1_cover_ != nullptr) {
          blind1_cover_->position = 1.0f - val[0] / 200.0f;
          blind1_cover_->publish_state();
        }
        break;
      case 0x13:  // blind 1 tilt (len 1)
        if (vlen >= 1 && blind1_cover_ != nullptr) {
          blind1_cover_->tilt = 0.5f - static_cast<int8_t>(val[0]) / 160.0f;
          blind1_cover_->publish_state();
        }
        break;
      case 0x14:  // blind 2 position (len 1)
        if (vlen >= 1 && blind2_cover_ != nullptr) {
          blind2_cover_->position = 1.0f - val[0] / 200.0f;
          blind2_cover_->publish_state();
        }
        break;
      case 0x15:  // blind 2 tilt (len 1)
        if (vlen >= 1 && blind2_cover_ != nullptr) {
          blind2_cover_->tilt = 0.5f - static_cast<int8_t>(val[0]) / 160.0f;
          blind2_cover_->publish_state();
        }
        break;
      default:
        break;
    }
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
  if (parent_ == nullptr)
    return;

  // Build a control frame: 23 fd ff 1c 10 01 00 02 <L> <TLV> <checksum>
  // TLV: [tag][len=6][block_type][sub_type][pos1][tilt1][pos2][tilt2]
  //   block_type: 0x28=stopped, 0x0a=moving
  //   sub_type: 0x08=stopped, 0x01=moving-down (close), 0x09=moving-up (open)
  //   pos: 0=open, 200=closed
  //   tilt: -80 to 80 (signed 8-bit)

  uint8_t block_type = 0x28;  // stopped by default
  uint8_t sub_type = 0x08;
  uint8_t position = 0;  // 0=open, 200=closed
  int8_t tilt = 0;       // -80 to 80

  // Check for position command first
  if (call.get_position().has_value()) {
    float target = call.get_position().value();
    if (target >= 1.0f) {
      // Open
      block_type = 0x0a;
      sub_type = 0x09;
      position = 0;
    } else if (target <= 0.0f) {
      // Close
      block_type = 0x0a;
      sub_type = 0x01;
      position = 200;
    }
  }

  // Check for stop command (overrides position)
  if (call.get_stop()) {
    block_type = 0x28;
    sub_type = 0x08;
    position = 0;
  }

  // Check for tilt command
  if (call.get_tilt().has_value()) {
    block_type = 0x28;
    sub_type = 0x08;
    float target_tilt = call.get_tilt().value();
    tilt = static_cast<int8_t>(target_tilt * 160.0f - 80.0f);  // 0-100% -> -80 to 80
  }

  uint8_t tag = this->tag_;
  uint8_t block_len = 6;

  uint8_t payload[8];
  payload[0] = tag;
  payload[1] = block_len;
  payload[2] = block_type;
  payload[3] = sub_type;
  payload[4] = position;
  payload[5] = static_cast<uint8_t>(tilt);
  payload[6] = 0;  // pos2
  payload[7] = 0;  // tilt2

  uint8_t frame[18];
  frame[0] = 0x23;  // magic
  frame[1] = 0xfd;  // class (covers)
  frame[2] = 0xff;  // direction (data)
  frame[3] = 0x1c;  // frame type (data)
  frame[4] = 0x10;  // data flag
  frame[5] = 0x01;  // dataset
  frame[6] = 0x00;  // dataset selector
  frame[7] = 0x02;  // dataset id for covers
  frame[8] = 8;     // L = payload length (8 bytes TLV)
  for (int i = 0; i < 8; i++) {
    frame[9 + i] = payload[i];
  }
  uint8_t checksum = 0;
  for (int i = 0; i < 17; i++) {
    checksum += frame[i];
  }
  frame[17] = checksum;

  parent_->write_frame(frame, 18);
  ESP_LOGD(TAG, "Sent cover control (tag=0x%02x): block_type=0x%02x sub_type=0x%02x pos=%d tilt=%d", tag, block_type,
           sub_type, position, tilt);
}

}  // namespace wisotronic
}  // namespace esphome
