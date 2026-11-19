#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace wisotronic {

// Macro to define sensors
#define WISOTRONIC_SENSOR(name, value_type) \
 protected: \
  sensor::Sensor *name##_{nullptr}; \
\
 public: \
  void set_##name(sensor::Sensor *name) { this->name##_ = name; }

// Macro to define covers
#define WISOTRONIC_COVER(name) \
 protected: \
  cover::Cover *name##_{nullptr}; \
\
 public: \
  void set_##name(cover::Cover *name) { this->name##_ = name; }

class WisotronicCustomComponent : public uart::UARTDevice, public Component {
 public:
  WisotronicCustomComponent() = default;
  explicit WisotronicCustomComponent(uart::UARTComponent *parent);

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // --------------------------------------------------------------------------
  // Sensors
  // --------------------------------------------------------------------------
  WISOTRONIC_SENSOR(control_unit_temperature_sensor, float)
  WISOTRONIC_SENSOR(outdoor_temperature_sensor, float)
  WISOTRONIC_SENSOR(indoor_temperature_sensor, float)
  WISOTRONIC_SENSOR(indoor_humidity_sensor, float)
  WISOTRONIC_SENSOR(wind_speed_sensor, float)
  WISOTRONIC_SENSOR(rain_sensor, float)
  WISOTRONIC_SENSOR(photo1_brightness_sensor, float)
  WISOTRONIC_SENSOR(photo2_brightness_sensor, float)
  WISOTRONIC_SENSOR(photo3_brightness_sensor, float)
  WISOTRONIC_SENSOR(twilight_brightness_sensor, float)

  // --------------------------------------------------------------------------
  // Covers
  // --------------------------------------------------------------------------
  WISOTRONIC_COVER(awning_cover)
  WISOTRONIC_COVER(window_cover)
  WISOTRONIC_COVER(blind1_cover)
  WISOTRONIC_COVER(blind2_cover)

 protected:
  void stream_to_packet(uint8_t byte);
  void parse_wisotronic_packet(uint8_t len);
  void process_tlv(uint8_t class_byte, uint8_t tag, uint8_t vlen, uint8_t *val);

  uint8_t buffer_[128];
  uint8_t p_buffer_ = 0;
};

class WisotronicCover : public cover::Cover, public Component {
 public:
  void set_parent(WisotronicCustomComponent *parent) { parent_ = parent; }

 protected:
  void control(const cover::CoverCall &call) override;
  cover::CoverTraits get_traits() override;

  WisotronicCustomComponent *parent_;
};

}  // namespace wisotronic
}  // namespace esphome
