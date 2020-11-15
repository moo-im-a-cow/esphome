#include "esphome/core/log.h"
#include "tuya_light.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.light";

void TuyaLight::setup() {
  if (this->dimmer_id_.has_value()) {
    this->parent_->register_listener(*this->dimmer_id_, [this](TuyaDatapoint datapoint) {
      auto call = this->state_->make_call();
      call.set_brightness(float(datapoint.value_uint) / this->max_value_);
      call.perform();
    });
  }
  if (switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, [this](TuyaDatapoint datapoint) {
      auto call = this->state_->make_call();
      call.set_state(datapoint.value_bool);
      call.perform();
    });
  }
    if (this->cct_id_.has_value()) {
      this->parent_->register_listener(*this->cct_id_, [this](TuyaDatapoint datapoint) {
        float ctf = *this->cold_white_temperature_ + float(datapoint.value_uint)*(*this->warm_white_temperature_-*this->cold_white_temperature_)/100;
        ESP_LOGCONFIG(TAG, "   CCT: %d %f", datapoint.value_uint, ctf);
        auto call = this->state_->make_call();
        call.set_color_temperature(ctf);
        call.perform();
      });
    }
  if (min_value_datapoint_id_.has_value()) {
    TuyaDatapoint datapoint{};
    datapoint.id = *this->min_value_datapoint_id_;
    datapoint.type = TuyaDatapointType::INTEGER;
    datapoint.value_int = this->min_value_;
    parent_->set_datapoint_value(datapoint);
  }
}

void TuyaLight::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Dimmer:");
  if (this->dimmer_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Dimmer has datapoint ID %u", *this->dimmer_id_);
  if (this->switch_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Switch has datapoint ID %u", *this->switch_id_);
  if (this->cct_id_.has_value())
    ESP_LOGCONFIG(TAG, "   CCT has datapoint ID %u", *this->cct_id_);
}

light::LightTraits TuyaLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supports_brightness(this->dimmer_id_.has_value());
  if(this->cct_id_.has_value()){
    traits.set_supports_color_temperature(true);
    traits.set_min_mireds(*this->cold_white_temperature_);
    traits.set_max_mireds(*this->warm_white_temperature_);
  }
  return traits;
}

void TuyaLight::setup_state(light::LightState *state) { state_ = state; }

void TuyaLight::write_state(light::LightState *state) {
  float brightness;
  state->current_values_as_brightness(&brightness);

  float ct =   state->current_values.get_color_temperature();
  ct=ct-*this->cold_white_temperature_;
  ct=ct/(*this->warm_white_temperature_-*this->cold_white_temperature_)*100;
  int cti = round(ct);
  cti = constrain(cti, this->min_value_, this->max_value_);
  ESP_LOGCONFIG(TAG, "%f %f %f %d", *this->cold_white_temperature_, *this->warm_white_temperature_, ct, cti);

  if (brightness == 0.0f) {
    // turning off, first try via switch (if exists), then dimmer
    if (switch_id_.has_value()) {
      TuyaDatapoint datapoint{};
      datapoint.id = *this->switch_id_;
      datapoint.type = TuyaDatapointType::BOOLEAN;
      datapoint.value_bool = false;

      parent_->set_datapoint_value(datapoint);
    } else if (dimmer_id_.has_value()) {
      TuyaDatapoint datapoint{};
      datapoint.id = *this->dimmer_id_;
      datapoint.type = TuyaDatapointType::INTEGER;
      datapoint.value_int = 0;
      parent_->set_datapoint_value(datapoint);
    }
    return;
  }

  auto brightness_int = static_cast<uint32_t>(brightness * this->max_value_);
  brightness_int = std::max(brightness_int, this->min_value_);

  auto cct_int = static_cast<uint32_t>(1 * this->max_value_);
  cct_int = std::max(cct_int, this->min_value_);
  if (this->dimmer_id_.has_value()) {
    TuyaDatapoint datapoint{};
    datapoint.id = *this->dimmer_id_;
    datapoint.type = TuyaDatapointType::INTEGER;
    datapoint.value_int = brightness_int;
    parent_->set_datapoint_value(datapoint);
  }
  if (this->cct_id_.has_value()) {
    TuyaDatapoint datapoint{};
    datapoint.id = *this->cct_id_;
    datapoint.type = TuyaDatapointType::INTEGER;
    datapoint.value_int = cti;
    parent_->set_datapoint_value(datapoint);
  }
  if (this->switch_id_.has_value()) {
    TuyaDatapoint datapoint{};
    datapoint.id = *this->switch_id_;
    datapoint.type = TuyaDatapointType::BOOLEAN;
    datapoint.value_bool = true;
    parent_->set_datapoint_value(datapoint);
  }
}

}  // namespace tuya
}  // namespace esphome
