// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_INPUTSYSTEM_ANALOGCODE_H_
#define SOURCE_INPUTSYSTEM_ANALOGCODE_H_

#include <cstdint>
#include "inputsystem/InputEnums.h"

// Get at joystick codes.
#define JOYSTICK_AXIS_INTERNAL(_joystick, _axis) \
  (JOYSTICK_FIRST_AXIS + ((_joystick)*MAX_JOYSTICK_AXES) + (_axis))

// Enumeration for analog input devices. Includes joysticks, mouse wheel,
// mouse.
enum AnalogCode_t {
  ANALOG_CODE_INVALID = -1,
  MOUSE_X = 0,
  MOUSE_Y,
  MOUSE_XY,  // Invoked when either x or y changes
  MOUSE_WHEEL,

  JOYSTICK_FIRST_AXIS,
  JOYSTICK_LAST_AXIS =
      JOYSTICK_AXIS_INTERNAL(MAX_JOYSTICKS - 1, MAX_JOYSTICK_AXES - 1),

  ANALOG_CODE_LAST,
};

inline constexpr AnalogCode_t JOYSTICK_AXIS(int32_t joystick, int32_t axis) {
  return static_cast<AnalogCode_t>(JOYSTICK_AXIS_INTERNAL(joystick, axis));
}

#endif  // SOURCE_INPUTSYSTEM_ANALOGCODE_H_
