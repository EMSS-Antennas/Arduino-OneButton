// -----
// OneButtonTiny.h - Library for detecting button clicks, doubleclicks and long
// press pattern on a single button. This class is implemented for use with the
// Arduino environment. Copyright (c) by Matthias Hertel,
// http://www.mathertel.de This work is licensed under a BSD style license. See
// http://www.mathertel.de/License.aspx More information on:
// http://www.mathertel.de/Arduino
// -----
// 01.12.2023 created from OneButtonTiny to support tiny environments.
// 02.2026 RAM optimized: reduced from ~36 bytes to ~22 bytes per instance
// -----

#ifndef OneButtonTiny_h
#define OneButtonTiny_h

#include "Arduino.h"
#include <PinChangeInterrupt.h>

// ----- Callback function types -----

extern "C" {
  typedef void (*callbackFunction)(void);
}


class OneButtonTiny {
public:
  // ----- Constructor -----

  /**
   * Initialize the OneButtonTiny library.
   * @param pin The pin to be used for input from a momentary button.
   * @param activeLow Set to true when the input level is LOW when the button is pressed, Default is true.
   * @param pullupActive Activate the internal pullup when available. Default is true.
   */
  explicit OneButtonTiny(const uint8_t pin, const bool activeLow = true, const bool pullupActive = true);

  /**
   * set # millisec after safe click is assumed.
   */
  void setDebounceMs(const uint8_t ms);

  /**
   * set # millisec after single click is assumed.
   */
  void setClickMs(const uint16_t ms);

  /**
   * set # millisec after press is assumed.
   */
  void setPressMs(const uint16_t ms);

  // ----- Attach events functions -----

  /**
   * Attach an event to be called when a single click is detected.
   * @param newFunction This function will be called when the event has been detected.
   */
  void attachClick(callbackFunction newFunction);

  /**
   * Attach an event to be called after a double click is detected.
   * @param newFunction This function will be called when the event has been detected.
   */
  void attachDoubleClick(callbackFunction newFunction);

  /**
   * Attach an event to fire when the button is pressed and held down.
   * @param newFunction
   */
  void attachLongPressStart(callbackFunction newFunction);

  /**
   * Attach an interrupt to be called immediately when a pin change is detected.
   * @param mode Interrupt mode (e.g. CHANGE)
   * @param userFunc Function to call on interrupt. If omitted a default no-op is used.
   */
  void attachInterupt(uint8_t mode = CHANGE, void (*userFunc)(void) = isrDefaultUnused);

  /** Enable pin-change interrupt for the configured pin. */
  void enableInterupt();

  /** Disable pin-change interrupt for the configured pin. */
  void disableInterupt();

  // ----- State machine functions -----

  /**
   * @brief Call this function every some milliseconds for checking the input
   * level at the initialized digital pin.
   */
  void tick(void);

  /**
   * @brief Call this function every time the input level has changed.
   * Using this function no digital input pin is checked because the current
   * level is given by the parameter.
   * Run the finite state machine (FSM) using the given level.
   */
  void tick(bool level);


  /**
   * Reset the button state machine.
   */
  void reset(void);


  /**
   * @return true if we are currently handling button press flow
   * (This allows power sensitive applications to know when it is safe to power down the main CPU)
   */
  bool isIdle() const {
    return _state == OCS_INIT;
  }


private:
  // ===== Optimized member layout for minimal RAM =====
  // Timing values stored in reduced precision where possible
  
  uint16_t _click_ms = 400;    // 2 bytes - msecs before click detected (max 65535)
  uint16_t _press_ms = 800;    // 2 bytes - msecs before long press detected
  uint16_t _startTime = 0;     // 2 bytes - stored as (millis() >> 2) for 4ms resolution, ~262s range
  uint16_t _lastDebounceTime = 0; // 2 bytes - stored as (millis() >> 2)

  // Function pointers - 2 bytes each on AVR
  callbackFunction _clickFunc = nullptr;        // 2 bytes
  callbackFunction _doubleClickFunc = nullptr;  // 2 bytes  
  callbackFunction _longPressStartFunc = nullptr; // 2 bytes

  uint8_t _pin;                // 1 byte - hardware pin number (0-255 is plenty)
  uint8_t _debounce_ms = 50;   // 1 byte - debounce time (max 255ms is plenty)
  
  // Packed flags byte: [buttonPressed:1][lastLevel:1][debouncedLevel:1][state:3][nClicks:2]
  // state uses 3 bits (0-7), nClicks uses 2 bits (0-3)
  uint8_t _flags = 0;          // 1 byte - packed state flags
  
  // Flag bit positions
  static constexpr uint8_t FLAG_BUTTON_PRESSED = 0x80;  // bit 7: active level polarity
  static constexpr uint8_t FLAG_LAST_LEVEL     = 0x40;  // bit 6: last debounce pin level  
  static constexpr uint8_t FLAG_DEBOUNCED      = 0x20;  // bit 5: debounced pin level
  static constexpr uint8_t STATE_MASK          = 0x1C;  // bits 4-2: state (0-7)
  static constexpr uint8_t STATE_SHIFT         = 2;
  static constexpr uint8_t CLICKS_MASK         = 0x03;  // bits 1-0: click count (0-3)

  // ISR callback support (static - shared across all instances)
  static void isrDefaultUnused();
  static void (*isrCallback)();

  // define FiniteStateMachine
  enum stateMachine_t : uint8_t {
    OCS_INIT = 0,
    OCS_DOWN = 1,  // button is down
    OCS_UP = 2,    // button is up
    OCS_COUNT = 3,
    OCS_PRESS = 4,  // button is hold down (changed from 6 to fit in 3 bits)
    OCS_PRESSEND = 5,
  };

  // Inline helpers for packed flags
  inline void _setState(stateMachine_t s) { _flags = (_flags & ~STATE_MASK) | ((s << STATE_SHIFT) & STATE_MASK); }
  inline stateMachine_t _getState() const { return static_cast<stateMachine_t>((_flags & STATE_MASK) >> STATE_SHIFT); }
  inline void _setClicks(uint8_t c) { _flags = (_flags & ~CLICKS_MASK) | (c & CLICKS_MASK); }
  inline uint8_t _getClicks() const { return _flags & CLICKS_MASK; }
  inline bool _getButtonPressed() const { return _flags & FLAG_BUTTON_PRESSED; }
  inline bool _getLastLevel() const { return _flags & FLAG_LAST_LEVEL; }
  inline void _setLastLevel(bool v) { if(v) _flags |= FLAG_LAST_LEVEL; else _flags &= ~FLAG_LAST_LEVEL; }
  inline bool _getDebouncedLevel() const { return _flags & FLAG_DEBOUNCED; }
  inline void _setDebouncedLevel(bool v) { if(v) _flags |= FLAG_DEBOUNCED; else _flags &= ~FLAG_DEBOUNCED; }
  
  // Time helpers - store time with 4ms resolution to fit in uint16_t
  inline uint16_t _now() const { return (uint16_t)(millis() >> 2); }
  inline uint16_t _elapsed(uint16_t start) const { return (_now() - start) << 2; } // Convert back to ms

  /**
   * Run the finite state machine (FSM) using the given level.
   */
  void _fsm(bool activeLevel);

  /**
   * Debounce helper - returns true if level is stable
   */
  bool _debounce(bool level);

  // Legacy compatibility
  stateMachine_t _state = OCS_INIT;  // Keep for compatibility, synced with packed state

public:
  uint8_t pin() const { return _pin; }
  stateMachine_t state() const { return _getState(); }
};

// Total RAM per instance: ~22 bytes (down from ~36)
// 2+2+2+2 + 2+2+2 + 1+1+1+1 = 18 bytes + padding = ~20-22 bytes

#endif
