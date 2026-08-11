#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"

#include <RF24.h>

#include <string>
#include <vector>

// Buffer size is given by hardware, do not change it unless you know what
// you're doing.
#define NRF24_MAX_PAYLOAD 32

namespace esphome {
namespace nrf24 {

class NRF24Component : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_ce_pin(InternalGPIOPin *ce_pin) { this->ce_pin_ = ce_pin; }
  void set_csn_pin(InternalGPIOPin *csn_pin) { this->csn_pin_ = csn_pin; }
  void set_irq_pin(InternalGPIOPin *irq_pin) { this->irq_pin_ = irq_pin; }

  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_pa_level(uint8_t pa_level) { this->pa_level_ = pa_level; }
  void set_data_rate(uint8_t data_rate) { this->data_rate_ = data_rate; }

  void set_tx_address(const std::vector<uint8_t> &address) { this->tx_address_ = address; }
  void set_rx_address(const std::vector<uint8_t> &address) { this->rx_address_ = address; }
  void set_rx_pipe(uint8_t pipe) { this->rx_pipe_ = pipe; }
  void set_rx_enabled(bool enabled) { this->rx_enabled_ = enabled; }

  void add_on_receive_callback(std::function<void(std::string)> &&callback);

  // Transmit a payload. Returns true on successful acknowledgement.
  bool send(const std::string &payload);

 protected:
  InternalGPIOPin *ce_pin_{nullptr};
  InternalGPIOPin *csn_pin_{nullptr};
  InternalGPIOPin *irq_pin_{nullptr};

  uint8_t channel_{76};
  uint8_t pa_level_{RF24_PA_LOW};
  uint8_t data_rate_{RF24_1MBPS};

  std::vector<uint8_t> tx_address_{0xDE, 0xAD, 0xC0, 0xDE, 0x01};
  std::vector<uint8_t> rx_address_{0xDE, 0xAD, 0xC0, 0xDE, 0x01};
  uint8_t rx_pipe_{1};
  bool rx_enabled_{false};

  CallbackManager<void(std::string)> receive_callback_;

  RF24 radio_;

  volatile bool message_available_{false};
  bool use_irq_{false};

  static void handle_irq_(NRF24Component *component);
  void process_incoming_();
};

class NRF24OnReceiveTrigger : public Trigger<std::string> {
 public:
  explicit NRF24OnReceiveTrigger(NRF24Component *component) {
    component->add_on_receive_callback([this](const std::string &value) { this->trigger(value); });
  }
};

template<typename... Ts> class NRF24SendAction : public Action<Ts...> {
 public:
  explicit NRF24SendAction(NRF24Component *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, payload)

  void play(Ts... x) override { this->parent_->send(this->payload_.value(x...)); }

 protected:
  NRF24Component *parent_;
};

}  // namespace nrf24
}  // namespace esphome
