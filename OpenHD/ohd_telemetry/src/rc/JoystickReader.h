//
// Created by consti10 on 22.08.22.
//
#pragma once

#include <array>
#include <functional>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

#include "openhd_spdlog.h"
#include "openhd_util.h"

/**
 * Hides away all the (nasty) "SDL Stuff"
 * The Paradigm of this class is similar to how for example external devices
 * are handled in general in OpenHD: If the user says he wants RC joystick
 * control, try to open the joystick and read data, re-connect if anything goes
 * wrong during run time. This class does all the connecting and handles
 * disconnecting and reading values in its own thread - you can query a "state"
 * from any thread at any time though. Theoretically, we could just use this
 * thread also for sending the RC data via mavlink - but this is a bit
 * dangerous, since I don't completely trust SDL yet (in regards to
 * disconnecting joysticks).
*/

enum class SwitchType {
  TWO_POS,
  THREE_POS
};

struct SwitchConfig {
  SwitchType type;              // тип тумблера
  std::vector<uint8_t> buttons; // кнопки, які його формують
  uint16_t channel;             // канал PWM
};

// Конфігурація тумблерів
static std::map<uint8_t, SwitchConfig> switches = {
  {0, {SwitchType::TWO_POS,   {0},        5}},
  {1, {SwitchType::THREE_POS, {1, 2, 3},  6}},
  {3, {SwitchType::THREE_POS, {4, 5, 6},  7}},
  {4, {SwitchType::THREE_POS, {7, 8, 9},  8}},
  {5, {SwitchType::THREE_POS, {10, 11, 12},  9}},
  {6, {SwitchType::THREE_POS, {13, 14, 15},  10}},
  {7, {SwitchType::THREE_POS, {16, 17, 18},  11}},
  {8, {SwitchType::TWO_POS, {19},  12}},
};


// Стан кнопок
static std::map<uint8_t, bool> button_states;   // стабільний стан

class JoystickReader {
 public:
  // See mavlink RC override
  // https://mavlink.io/en/messages/common.html#RC_CHANNELS_OVERRIDE
  static constexpr uint16_t DEFAULT_RC_CHANNELS_VALUE = UINT16_MAX;
  // the rc channel override message(s) support 18 values, so we do so, too
  static constexpr auto N_CHANNELS = 18;
  // We use the first 8 Channels for "axis" joystick values
  static constexpr auto N_CHANNELS_RESERVED_FOR_AXES = 4;
  static constexpr auto N_CHANNELS_RESERVED_FOR_BUTTONS = 8;
  static constexpr auto N_CHANNELS_RESERVED_FOR_SLIDERS = 4;
  static constexpr uint16_t VALUE_BUTTON_UP = 2000;
  static constexpr uint16_t VALUE_BUTTON_MIDDLE = 1500;
  static constexpr uint16_t VALUE_BUTTON_DOWN = 1000;
  struct CurrChannelValues {
    std::array<uint16_t, N_CHANNELS> values{DEFAULT_RC_CHANNELS_VALUE};
    // Time point when we received the last update to at least one of the
    // channel(s)
    std::chrono::steady_clock::time_point last_update;
    // Weather we think the RC (joystick) is currently connected or not.
    bool considered_connected = false;
    // the name of the joystick
    std::string joystick_name = "unknown";
  };
  explicit JoystickReader();
  ~JoystickReader();
  // Get the current "state", thread-safe
  CurrChannelValues get_current_state();
  // For debugging
  static std::string curr_state_to_string(
      const CurrChannelValues& curr_channel_values);

 private:
  void loop();
  void connect_once_and_read_until_error();
  // Wait up to timeout_ms for an event, and then read as many events as there
  // are available We are only interested in the Joystick events
  void wait_for_events(int timeout_ms);
  int process_event(void* event, std::array<uint16_t, N_CHANNELS>& values);
  void reset_curr_values();
  std::unique_ptr<std::thread> m_read_joystick_thread;
  bool terminate = false;
  std::mutex m_curr_values_mutex;
  CurrChannelValues m_curr_values;
  std::shared_ptr<spdlog::logger> m_console;

 private:
  void write_matching_axis(
      std::array<uint16_t, JoystickReader::N_CHANNELS>& rc_data,
      uint8_t axis_index, int16_t value);
  static void write_matching_button(std::array<uint16_t, 18>& rc_data,
                                    uint8_t button, bool up);
  static int calc_pwm_for_switch(const SwitchConfig& sw);
};
