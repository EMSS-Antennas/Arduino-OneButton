/**
 * @file OneButtonTiny.cpp
 *
 * @brief Library for detecting button clicks, doubleclicks and long press
 * pattern on a single button. RAM-optimized version.
 *
 * @author Matthias Hertel, https://www.mathertel.de
 * @Copyright Copyright (c) by Matthias Hertel, https://www.mathertel.de.
 *            Ihor Nehrutsa, Ihor.Nehrutsa@gmail.com
 *            RAM optimization by EMSS-Antennas, 2026
 *
 * This work is licensed under a BSD style license. See
 * http://www.mathertel.de/License.aspx
 */

#include "OneButtonTiny.h"

// ISR callback defaults (static - shared across all instances)
void (*OneButtonTiny::isrCallback)() = OneButtonTiny::isrDefaultUnused;
void OneButtonTiny::isrDefaultUnused() { /* NOP */ }


// ----- Constructor -----
OneButtonTiny::OneButtonTiny(const uint8_t pin, const bool activeLow, const bool pullupActive) 
    : _pin(pin), _flags(0), _state(OCS_INIT)
{
  // Set active level polarity in flags
  if (activeLow) {
    // button connects pin to GND when pressed - active level is LOW
    // So we DON'T set FLAG_BUTTON_PRESSED (means active=LOW)
  } else {
    // button connects pin to VCC when pressed - active level is HIGH
    _flags |= FLAG_BUTTON_PRESSED;
  }

  // Configure pin mode
  pinMode(pin, pullupActive ? INPUT_PULLUP : INPUT);
}


// ----- Configuration setters -----

void OneButtonTiny::setDebounceMs(const uint8_t ms) {
  _debounce_ms = ms;
}

void OneButtonTiny::setClickMs(const uint16_t ms) {
  _click_ms = ms;
}

void OneButtonTiny::setPressMs(const uint16_t ms) {
  _press_ms = ms;
}


// ----- Callback attachment -----

void OneButtonTiny::attachClick(callbackFunction newFunction) {
  _clickFunc = newFunction;
}

void OneButtonTiny::attachDoubleClick(callbackFunction newFunction) {
  _doubleClickFunc = newFunction;
}

void OneButtonTiny::attachLongPressStart(callbackFunction newFunction) {
  _longPressStartFunc = newFunction;
}


// ----- Interrupt support -----

void OneButtonTiny::attachInterupt(uint8_t mode, void (*userFunc)(void)) {
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(_pin), userFunc, mode);
  isrCallback = userFunc;
}

void OneButtonTiny::enableInterupt() {
  enablePinChangeInterrupt(digitalPinToPinChangeInterrupt(_pin));
}

void OneButtonTiny::disableInterupt() {
  disablePinChangeInterrupt(digitalPinToPinChangeInterrupt(_pin));
}


// ----- State machine -----

void OneButtonTiny::reset(void) {
  _setState(OCS_INIT);
  _setClicks(0);
  _startTime = 0;
  _state = OCS_INIT;  // Keep legacy var in sync
}


bool OneButtonTiny::_debounce(bool level) {
  uint16_t now = _now();
  
  if (_getLastLevel() == level) {
    // Level unchanged - check if debounce time elapsed
    if ((uint16_t)(now - _lastDebounceTime) >= (uint8_t)(_debounce_ms >> 2)) {
      _setDebouncedLevel(level);
    }
  } else {
    // Level changed - restart debounce timer
    _lastDebounceTime = now;
    _setLastLevel(level);
  }
  return _getDebouncedLevel();
}


void OneButtonTiny::tick(void) {
  // Read pin and check if it matches the "pressed" level
  bool rawLevel = digitalRead(_pin);
  bool activeLevel = _getButtonPressed() ? rawLevel : !rawLevel;
  _fsm(_debounce(activeLevel));
}


void OneButtonTiny::tick(bool activeLevel) {
  _fsm(_debounce(activeLevel));
}


void OneButtonTiny::_fsm(bool activeLevel) {
  uint16_t now = _now();
  uint16_t waitTime = (now - _startTime) << 2;  // Convert back to ms
  stateMachine_t currentState = _getState();

  switch (currentState) {
    case OCS_INIT:
      // Waiting for button press
      if (activeLevel) {
        _setState(OCS_DOWN);
        _startTime = now;
        _setClicks(0);
      }
      break;

    case OCS_DOWN:
      // Button is down, waiting for release or long press timeout
      if (!activeLevel) {
        // Button released
        _setState(OCS_UP);
        _startTime = now;
      } else if (waitTime > _press_ms) {
        // Long press detected
        if (_longPressStartFunc) _longPressStartFunc();
        _setState(OCS_PRESS);
      }
      break;

    case OCS_UP:
      // Button just released - count the click
      _setClicks(_getClicks() + 1);
      _setState(OCS_COUNT);
      break;

    case OCS_COUNT:
      // Debounce complete, counting clicks
      if (activeLevel) {
        // Button pressed again (for double-click detection)
        _setState(OCS_DOWN);
        _startTime = now;
      } else if (waitTime >= _click_ms || _getClicks() >= 2) {
        // Timeout or max clicks reached - fire callback
        uint8_t clicks = _getClicks();
        if (clicks == 1 && _clickFunc) {
          _clickFunc();
        } else if (clicks >= 2 && _doubleClickFunc) {
          _doubleClickFunc();
        }
        reset();
      }
      break;

    case OCS_PRESS:
      // In long press state, waiting for release
      if (!activeLevel) {
        _setState(OCS_PRESSEND);
        _startTime = now;
      }
      break;

    case OCS_PRESSEND:
      // Long press ended
      reset();
      break;

    default:
      // Unknown state - reset
      reset();
      break;
  }
  
  // Keep legacy _state in sync
  _state = _getState();
}

// end.


