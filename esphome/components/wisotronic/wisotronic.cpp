#include "wisotronic.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>

namespace esphome {
namespace wisotronic {

static const char TAG[] PROGMEM = "wisotronic";

WisotronicCustomComponent::WisotronicCustomComponent(uart::UARTComponent *parent) : UARTDevice(parent) {}

void WisotronicCustomComponent::setup() {
  // Initialize cover state tracking
  for (int i = 0; i < 4; i++) {
    setpoints_[i].sub_type = 0x08;  // stopped
    setpoints_[i].position = 0;
    setpoints_[i].tilt = 0;
    actual_[i].sub_type = 0x08;
    actual_[i].position = 0;
    actual_[i].tilt = 0;
  }
  sequence_ = 0;
}

void WisotronicCustomComponent::loop() {
  while (available()) {
    uint8_t byte = read();
    stream_to_packet(byte);
  }
}

void WisotronicCustomComponent::write_frame(const uint8_t *frame, uint8_t len) {
  ESP_LOGI(TAG, "write_frame called, len=%d", len);

  // Wait for bus to be idle (no incoming data for at least 5ms)
  // At 19200 baud, a full frame (46 bytes) takes ~24ms to transmit.
  // The inter-frame gap is ~30ms. 5ms silence = frame boundary detected.
  uint32_t start_wait = millis();
  uint32_t last_data = millis();
  while (true) {
    if (available() > 0) {
      last_data = millis();  // Data received, reset quiet timer
    }
    if (millis() - last_data >= 5) {  // 5ms of silence = frame boundary
      break;
    }
    if (millis() - start_wait > 100) {  // Max wait 100ms (safety)
      ESP_LOGW(TAG, "TX timeout after 100ms, sending anyway");
      break;
    }
    delay(1);
  }

  // Small additional gap to ensure bus is quiet
  delayMicroseconds(2000);

  for (uint8_t i = 0; i < len; i++) {
    write(frame[i]);
  }
  ESP_LOGI(TAG, "write_frame completed, sent %d bytes", len);
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
  LOG_TEXT_SENSOR("", "Device Time", this->device_time_);
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
        if (vlen >= 6) {
          // Update actual state
          actual_[0].sub_type = val[0];
          actual_[0].position = val[2];
          actual_[0].tilt = static_cast<int8_t>(val[3]);
          if (awning_cover_ != nullptr) {
            awning_cover_->position = 1.0f - val[2] / 200.0f;
            awning_cover_->publish_state();
          }
        }
        break;
      case 0x01:  // window block (len 6)
        if (vlen >= 6) {
          // Update actual state
          actual_[1].sub_type = val[0];
          actual_[1].position = val[2];
          actual_[1].tilt = static_cast<int8_t>(val[3]);
          if (window_cover_ != nullptr) {
            window_cover_->position = val[2] / 200.0f;
            window_cover_->publish_state();
          }
        }
        break;
      case 0x02:  // blind block (len 6): block_type, sub_type, pos1, tilt1, pos2, tilt2
        if (vlen >= 6) {
          // Update actual state for both blinds
          actual_[2].sub_type = val[0];
          actual_[2].position = val[2];
          actual_[2].tilt = static_cast<int8_t>(val[3]);
          actual_[3].sub_type = val[0];
          actual_[3].position = val[4];
          actual_[3].tilt = static_cast<int8_t>(val[5]);
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
      case 0x11:  // timestamp (len 7): sec, min, hour|dst|ver, wday, day, month, year-2000
        if (vlen >= 7 && device_time_ != nullptr) {
          char buf[25];
          snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d", 2000 + val[6], val[5], val[4], val[2] & 0x3F,
                   val[1], val[0]);
          device_time_->publish_state(buf);
        }
        break;
      default:
        break;
    }
  }
}

void WisotronicCustomComponent::send_batch_frame() {
  // Build 46-byte batch frame (class 0xfd, dataset 2, L=36)
  // Header: 23 fd ff 1c 10 01 00 02 24
  // TLV: tag 0x00 (awning, 6), 0x01 (window, 6), 0x02 (blind1+2, 6), 0x03 (seq, 1), 0x11 (timestamp, 7)

  uint8_t frame[46];
  frame[0] = 0x23;  // magic
  frame[1] = 0xfd;  // class (covers)
  frame[2] = 0xff;  // direction (data)
  frame[3] = 0x1c;  // frame type (data)
  frame[4] = 0x10;  // data flag
  frame[5] = 0x01;  // dataset
  frame[6] = 0x00;  // dataset selector
  frame[7] = 0x02;  // dataset id (covers)
  frame[8] = 36;    // L = 36 bytes TLV payload

  uint8_t *payload = frame + 9;
  uint8_t offset = 0;

  // Tag 0x00: Awning (6 bytes)
  payload[offset++] = 0x00;  // tag
  payload[offset++] = 0x06;  // len
  payload[offset++] = 0x0a;  // block_type (moving)
  payload[offset++] = setpoints_[0].sub_type;
  payload[offset++] = setpoints_[0].position;
  payload[offset++] = static_cast<uint8_t>(setpoints_[0].tilt);

  // Tag 0x01: Window (6 bytes)
  payload[offset++] = 0x01;
  payload[offset++] = 0x06;
  payload[offset++] = 0x0a;
  payload[offset++] = setpoints_[1].sub_type;
  payload[offset++] = setpoints_[1].position;
  payload[offset++] = static_cast<uint8_t>(setpoints_[1].tilt);

  // Tag 0x02: Blind 1+2 (6 bytes) - pos1/tilt1 = blind1, pos2/tilt2 = blind2
  payload[offset++] = 0x02;
  payload[offset++] = 0x06;
  payload[offset++] = 0x0a;
  payload[offset++] = setpoints_[2].sub_type;
  payload[offset++] = setpoints_[2].position;
  payload[offset++] = static_cast<uint8_t>(setpoints_[2].tilt);
  payload[offset++] = setpoints_[3].position;
  payload[offset++] = static_cast<uint8_t>(setpoints_[3].tilt);

  // Tag 0x03: Sequence (1 byte)
  payload[offset++] = 0x03;
  payload[offset++] = 0x01;
  payload[offset++] = sequence_++;

  // Tag 0x11: Timestamp (7 bytes) - sec, min, hour|dst|ver, wday, day, month, year-2000
  // For now, use zeros (device fills in actual time)
  payload[offset++] = 0x11;
  payload[offset++] = 0x07;
  payload[offset++] = 0x00;  // sec
  payload[offset++] = 0x00;  // min
  payload[offset++] = 0x00;  // hour (with flags)
  payload[offset++] = 0x00;  // wday
  payload[offset++] = 0x00;  // day
  payload[offset++] = 0x00;  // month
  payload[offset++] = 0x00;  // year-2000

  // Calculate checksum
  uint8_t checksum = 0;
  for (int i = 0; i < 45; i++) {
    checksum += frame[i];
  }
  frame[45] = checksum;

  write_frame(frame, 46);
  ESP_LOGI(TAG, "Sent batch control frame (seq=%d)", sequence_ - 1);
}

cover::CoverTraits WisotronicCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_is_assumed_state(false);
  traits.set_supports_position(true);
  traits.set_supports_tilt(true);
  return traits;
}

void WisotronicCover::control(const cover::CoverCall &call) {
  ESP_LOGI(TAG, "control() called for tag=0x%02x, position=%.2f, tilt=%.2f, stop=%d", this->tag_,
           call.get_position().value_or(-1.0f), call.get_tilt().value_or(-1.0f), call.get_stop());

  if (parent_ == nullptr)
    return;

  // Update this cover's setpoint
  if (call.get_position().has_value()) {
    float target = call.get_position().value();
    parent_->setpoints_[this->tag_].position = static_cast<uint8_t>(target * 200.0f);
    if (target >= 1.0f) {
      parent_->setpoints_[this->tag_].sub_type = 0x09;  // open
    } else if (target <= 0.0f) {
      parent_->setpoints_[this->tag_].sub_type = 0x01;  // close
    } else {
      parent_->setpoints_[this->tag_].sub_type = 0x02;  // intermediate
    }
  }
  if (call.get_stop()) {
    parent_->setpoints_[this->tag_].sub_type = 0x08;  // stop
    parent_->setpoints_[this->tag_].position = parent_->actual_[this->tag_].position;
  }
  if (call.get_tilt().has_value()) {
    parent_->setpoints_[this->tag_].tilt = static_cast<int8_t>(call.get_tilt().value() * 160.0f - 80.0f);
  }

  // Build batch frame with all covers' setpoints
  parent_->send_batch_frame();
}

}  // namespace wisotronic
}  // namespace esphome
