#include "nrf24.h"
#include "esphome/core/log.h"

#include <SPI.h>

namespace esphome {
namespace nrf24 {

static const char *const TAG = "nrf24";

void IRAM_ATTR NRF24Component::handle_irq_(NRF24Component *component) { component->message_available_ = true; }

void NRF24Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up NRF24...");

  SPI.begin();

  this->radio_ = RF24(this->ce_pin_->get_pin(), this->csn_pin_->get_pin());

  if (!this->radio_.begin()) {
    ESP_LOGE(TAG, "NRF24 initialization failed, check wiring!");
    this->mark_failed();
    return;
  }

  this->radio_.setChannel(this->channel_);
  this->radio_.setPALevel(this->pa_level_);
  this->radio_.setDataRate(static_cast<rf24_datarate_e>(this->data_rate_));

  // Writing pipe used by send()
  this->radio_.openWritingPipe(this->tx_address_.data());

  if (this->rx_enabled_) {
    this->radio_.openReadingPipe(this->rx_pipe_, this->rx_address_.data());
    this->radio_.startListening();

    // Attach interrupt to IRQ pin if configured, otherwise fall back to polling
    if (this->irq_pin_ != nullptr) {
      this->use_irq_ = true;
      this->irq_pin_->attach_interrupt(NRF24Component::handle_irq_, this, gpio::INTERRUPT_FALLING_EDGE);
    }
  } else {
    this->radio_.stopListening();
  }
}

void NRF24Component::dump_config() {
  ESP_LOGCONFIG(TAG, "NRF24:");
  LOG_PIN("  CE Pin: ", this->ce_pin_);
  LOG_PIN("  CSN Pin: ", this->csn_pin_);
  LOG_PIN("  IRQ Pin: ", this->irq_pin_);
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  ESP_LOGCONFIG(TAG, "  PA Level: %u", this->pa_level_);
  ESP_LOGCONFIG(TAG, "  Data Rate: %u", this->data_rate_);
  ESP_LOGCONFIG(TAG, "  RX Enabled: %s", YESNO(this->rx_enabled_));
  if (this->rx_enabled_) {
    ESP_LOGCONFIG(TAG, "  RX Pipe: %u", this->rx_pipe_);
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup failed!");
  }
}

void NRF24Component::loop() {
  if (!this->rx_enabled_) {
    return;
  }

  // When using IRQ, only process after the interrupt flagged a message.
  // Otherwise poll the radio directly.
  if (this->use_irq_) {
    if (!this->message_available_) {
      return;
    }
    this->message_available_ = false;
  }

  this->process_incoming_();
}

void NRF24Component::process_incoming_() {
  while (this->radio_.available()) {
    uint8_t length = this->radio_.getPayloadSize();
    if (length == 0 || length > NRF24_MAX_PAYLOAD) {
      ESP_LOGW(TAG, "Invalid message size: %d", length);
      return;
    }

    char buffer[NRF24_MAX_PAYLOAD + 1];
    memset(buffer, 0, sizeof(buffer));

    this->radio_.read(&buffer, length);
    buffer[NRF24_MAX_PAYLOAD] = '\0';

    std::string message(buffer);
    ESP_LOGD(TAG, "Received message: %s", buffer);
    this->receive_callback_.call(message);
  }
}

void NRF24Component::add_on_receive_callback(std::function<void(std::string)> &&callback) {
  this->receive_callback_.add(std::move(callback));
}

bool NRF24Component::send(const std::string &payload) {
  if (this->is_failed()) {
    ESP_LOGW(TAG, "Cannot send, component failed to initialize");
    return false;
  }

  // Frame is a fixed-size, zero-padded buffer to stay compatible with the
  // legacy transmitter/receiver framing.
  char buffer[NRF24_MAX_PAYLOAD];
  memset(buffer, 0, sizeof(buffer));

  size_t length = payload.length();
  if (length > NRF24_MAX_PAYLOAD) {
    ESP_LOGW(TAG, "Payload too long (%u bytes), truncating to %d bytes", length, NRF24_MAX_PAYLOAD);
    length = NRF24_MAX_PAYLOAD;
  }
  memcpy(buffer, payload.c_str(), length);

  // Switch to TX mode for the duration of the write.
  if (this->rx_enabled_) {
    this->radio_.stopListening();
  }

  ESP_LOGD(TAG, "Sending message: %s", payload.c_str());
  bool result = this->radio_.write(&buffer, sizeof(buffer));

  if (result) {
    ESP_LOGD(TAG, "Message sent successfully");
  } else {
    ESP_LOGW(TAG, "Message send failed (no acknowledgement)");
  }

  // Return to listening mode if this node also receives.
  if (this->rx_enabled_) {
    this->radio_.startListening();
  }

  return result;
}

}  // namespace nrf24
}  // namespace esphome
