#include "_Plugin_Helper.h"

#ifdef USES_P126

// #######################################################################################################
// ################################ Plugin 126 74HC595 Shiftregisters      ###############################
// #######################################################################################################

/** Changelog:
 * 2022-01-22 tonhuisman: ShiftRegister74HC595_NonTemplate library: Add setSize method, cleanup constructor
 *                        Setting: Restore register-buffer state from RTC values after warm boot (or crash...)
 *                        NB:!!! Only restores up to 4 * VARS_PER_TASK (16) chip values, starting at the configured Offset for display !!!
 *                        When enabled, changing the offset will reset the values content to 0.
 *                        Code improvements and optimizations
 *                        Add command 74hcSetChipCount for changing the number of chips at runtime. Does not restart the plugin.
 * 2022-01-20 tonhuisman: Fix some bugs, optimize code, now actually supports 255 chips = 2048 pins
 *                        Hex Values display now in uppercase for readability
 * 2022-01-19 tonhuisman: Add 74hcSetOffset and 74hxSetHexBin commands
 * 2022-01-18 tonhuisman: Improve parsing for 74hcsetall with chipnumber (1..chipCount) and data width (1..4) options
 * 2022-01-17 tonhuisman: Extend to max. 255 chips, add offset for display values, add 74hcSetAllNoUpdate command
 *                        Rename Value names
 * 2022-01-16 tonhuisman: Refactor ShiftRegister74HC595 to ShiftRegister74HC595_NonTemplate to enable runtime sizing
 *                        Add commands, implement PLUGIN_WEBFORM_SHOW_VALUES, testing and improving
 * 2022-01-15 tonhuisman: Implement command handling
 * 2021-11-17 tonhuisman: Initial plugin development. Based on a Forum request: https://www.letscontrolit.com/forum/viewtopic.php?f=5&t=8751
 */

/** Commands:
 * 74hcSet,<pin>,<0|1>                        : Set a single pin on or off, and update.
 * 74hcSetNoUpdate,<pin>,<0|1>                : Set a single pin on or off. Use 74hcUpdate to set outputs.
 * 74hcUpdate                                 : Update all pin states to the registers.
 * 74hcSetAll,[chip:][width:]<value>...       : Set a range of chips with values, default 32 bit values (width 4).
 * 74hcSetAllNoUpdate,[chip:][width:]<value>  : Ditto, without immediate update. Use 74hcUpdate to set outputs.
 * 74hcSetAllLow                              : Set all register outputs to 0/low.
 * 74hcSetAllHigh                             : Set all register outputs to 1/high.
 * 74hcSetOffset,<chip offset>                : Set the chip offset for display. Will reflect in the device configuration, but not saved.
 * 74hcSetChipCount,<chip count>              : Set the number of chips to use, without restarting the plugin. Range 1..P126_MAX_CHIP_COUNT.
 * 74hcSetHexBin,<0|1>                        : Turn off/on the Hex or Bin Values display, reflected in device configuration, but not saved.
 */

# define PLUGIN_126
# define PLUGIN_ID_126          126
# define PLUGIN_NAME_126        "Output - 74HC595 Shiftregisters [TESTING]"
# define PLUGIN_VALUENAME1_126  "State_A"
# define PLUGIN_VALUENAME2_126  "State_B"
# define PLUGIN_VALUENAME3_126  "State_C"
# define PLUGIN_VALUENAME4_126  "State_D"

# include "./src/PluginStructs/P126_data_struct.h"

boolean Plugin_126(uint8_t function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      Device[++deviceCount].Number           = PLUGIN_ID_126;
      Device[deviceCount].Type               = DEVICE_TYPE_TRIPLE;
      Device[deviceCount].VType              = Sensor_VType::SENSOR_TYPE_SINGLE;
      Device[deviceCount].Ports              = 0;
      Device[deviceCount].PullUpOption       = false;
      Device[deviceCount].InverseLogicOption = false;
      Device[deviceCount].FormulaOption      = true;
      Device[deviceCount].ValueCount         =
      # if P126_MAX_CHIP_COUNT <= 4
        1
      # elif P126_MAX_CHIP_COUNT <= 8
        2
      # elif P126_MAX_CHIP_COUNT <= 12
        3
      # else // if P126_MAX_CHIP_COUNT <= 4
        4
      # endif // if P126_MAX_CHIP_COUNT <= 4
      ;
      Device[deviceCount].SendDataOption = true;
      Device[deviceCount].TimerOption    = true;
      Device[deviceCount].TimerOptional  = true;

      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_126);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_126));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_126));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_126));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_126));
      break;
    }

    case PLUGIN_SET_DEFAULTS:
    {
      P126_CONFIG_DATA_PIN                         = -1;
      P126_CONFIG_CLOCK_PIN                        = -1;
      P126_CONFIG_LATCH_PIN                        = -1;
      ExtraTaskSettings.TaskDeviceValueDecimals[0] = 0; // No decimals needed
      ExtraTaskSettings.TaskDeviceValueDecimals[1] = 0; // No decimals needed
      ExtraTaskSettings.TaskDeviceValueDecimals[2] = 0; // No decimals needed
      ExtraTaskSettings.TaskDeviceValueDecimals[3] = 0; // No decimals needed
      break;
    }

    case PLUGIN_GET_DEVICEGPIONAMES:
    {
      event->String1 = formatGpioName_output(F("Data pin (DS)"));
      event->String2 = formatGpioName_output(F("Clock pin (SH_CP)"));
      event->String3 = formatGpioName_output(F("Latch pin (ST_CP)"));
      break;
    }
    case PLUGIN_WEBFORM_LOAD:
    {
      addFormNumericBox(F("Number of chips (Q7' &rarr; DS)"),
                        F("p126_chips"),
                        P126_CONFIG_CHIP_COUNT,
                        1,                    // Minimum is 1 chip
                        P126_MAX_CHIP_COUNT); // Max chip count
      String unit = F("Daisychained 1..");
      unit += P126_MAX_CHIP_COUNT;
      addUnit(unit);

      addFormNumericBox(F("Offset for display"),
                        F("p126_offset"),
                        P126_CONFIG_SHOW_OFFSET,
                        0,
                        P126_MAX_SHOW_OFFSET);
      addUnit(F("Multiple of 4"));

      # ifdef P126_SHOW_VALUES
      addFormCheckBox(F("Values display (Off=Hex/On=Bin)"), F("p126_valuesdisplay"), P126_CONFIG_FLAGS_GET_VALUES_DISPLAY == 1);
      # endif // ifdef P126_SHOW_VALUES
      addFormCheckBox(F("Restore Values on warm boot"),     F("p126_valuesrestore"), P126_CONFIG_FLAGS_GET_VALUES_RESTORE);

      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      uint8_t previousOffset = P126_CONFIG_SHOW_OFFSET;
      P126_CONFIG_CHIP_COUNT  = getFormItemInt(F("p126_chips"));
      P126_CONFIG_SHOW_OFFSET = getFormItemInt(F("p126_offset"));

      if (P126_CONFIG_SHOW_OFFSET >= P126_CONFIG_CHIP_COUNT) {
        P126_CONFIG_SHOW_OFFSET = 0;
      }
      P126_CONFIG_SHOW_OFFSET -= (P126_CONFIG_SHOW_OFFSET % 4);

      if ((P126_CONFIG_CHIP_COUNT > 4) &&
          (P126_CONFIG_SHOW_OFFSET > P126_CONFIG_CHIP_COUNT - 4) &&
          (P126_CONFIG_CHIP_COUNT < P126_MAX_SHOW_OFFSET)) {
        P126_CONFIG_SHOW_OFFSET -= 4;
      }

      uint32_t lSettings = 0u;

      # ifdef P126_SHOW_VALUES

      if (isFormItemChecked(F("p126_valuesdisplay"))) { bitSet(lSettings, P126_FLAGS_VALUES_DISPLAY); }
      # endif // ifdef P126_SHOW_VALUES

      if (!isFormItemChecked(F("p126_valuesrestore"))) { bitSet(lSettings, P126_FLAGS_VALUES_RESTORE); } // Inverted setting!

      P126_CONFIG_FLAGS = lSettings;

      // Reset State_A..D values when changing the offset
      if ((previousOffset != P126_CONFIG_SHOW_OFFSET) && P126_CONFIG_FLAGS_GET_VALUES_RESTORE) {
        for (uint8_t varNr = 0; varNr < VARS_PER_TASK; varNr++) {
          UserVar.setUint32(event->TaskIndex, varNr, 0u);
        }
        # ifdef P126_DEBUG_LOG
        addLog(LOG_LEVEL_INFO, F("74HC595: 'Offset for display' changed: state values reset."));
        # endif // ifdef P126_DEBUG_LOG
      }

      success = true;
      break;
    }

    case PLUGIN_INIT:
    {
      initPluginTaskData(event->TaskIndex, new (std::nothrow) P126_data_struct(P126_CONFIG_DATA_PIN,
                                                                               P126_CONFIG_CLOCK_PIN,
                                                                               P126_CONFIG_LATCH_PIN,
                                                                               P126_CONFIG_CHIP_COUNT));
      P126_data_struct *P126_data = static_cast<P126_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr == P126_data) {
        return success;
      }

      if (P126_data->isInitialized()) {
        success = P126_data->plugin_init(event); // Optionally restore State_A..State_D values from RTC (on warm-boot only!)
      }

      if (success) {
        addLog(LOG_LEVEL_INFO, F("74HC595: Initialized."));
      } else {
        addLog(LOG_LEVEL_ERROR, F("74HC595: Initialization error!"));
      }

      break;
    }

    case PLUGIN_EXIT:
    {
      success = true;
      break;
    }

    case PLUGIN_READ:
    {
      P126_data_struct *P126_data = static_cast<P126_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr == P126_data) {
        return success;
      }

      success = P126_data->plugin_read(event); // Get state

      break;
    }

    # ifdef P126_SHOW_VALUES
    case PLUGIN_WEBFORM_SHOW_VALUES:
    {
      P126_data_struct *P126_data = static_cast<P126_data_struct *>(getPluginTaskData(event->TaskIndex));

      if ((nullptr != P126_data) && P126_data->isInitialized()) {                                   // Only show if plugin is active
        String state, label;
        state.reserve(40);
        String abcd = F("ABCDEFGH");                                                                // In case anyone dares to extend
                                                                                                    // VARS_PER_TASK to 8...
        uint64_t val;
        const uint16_t endCheck = P126_CONFIG_CHIP_COUNT + (P126_CONFIG_CHIP_COUNT == 255 ? 3 : 4); // 4(.0) = nr of bytes in an uint32_t.
        const uint16_t maxVar   = min(static_cast<uint8_t>(VARS_PER_TASK), static_cast<uint8_t>(ceil(P126_CONFIG_CHIP_COUNT / 4.0)));

        for (uint16_t varNr = 0; varNr < maxVar; varNr++) {
          if (P126_CONFIG_FLAGS_GET_VALUES_DISPLAY) {
            label = F("Bin");
            state = F("0b");
          } else {
            label = F("Hex");
            state = F("0x");
          }
          label += F(" State_");
          label += abcd.substring(varNr, varNr + 1);
          label += ' ';

          label += (P126_CONFIG_SHOW_OFFSET + (4 * varNr) + 1);          // 4 = nr of bytes in an uint32_t.
          label += '_';
          label += min(255, P126_CONFIG_SHOW_OFFSET + (4 * varNr) + 4);  // Limited to max 255 chips

          if ((P126_CONFIG_SHOW_OFFSET + (4 * varNr) + 4) <= endCheck) { // Only show if still in range
            val  = static_cast<uint64_t>(UserVar.getUint32(event->TaskIndex, varNr));
            val &= 0x0ffffffff;                                          // Keep 32 bits
            val |= 0x100000000;                                          // Set bit just left of 32 bits so we will see the
                                                                         // leading zeroes
            String valStr = ull2String(val, (P126_CONFIG_FLAGS_GET_VALUES_DISPLAY ? BIN : HEX));
            valStr.remove(0, 1);                                         // Delete leading 1 we added
            valStr.toUpperCase();                                        // uppercase hex for readability
            state += valStr;

            if (P126_CONFIG_FLAGS_GET_VALUES_DISPLAY) {                  // Insert readability separators for Bin display
              uint8_t dotInsert = 10;

              for (uint8_t i = 0; i < 3; i++, dotInsert += 9) {
                state = state.substring(0, dotInsert) + '.' + state.substring(dotInsert);
              }
            }
            pluginWebformShowValue(event->TaskIndex, VARS_PER_TASK + varNr, label, state, true);
          }
        }
      }
      break;
    }
    # endif // ifdef P126_SHOW_VALUES
    case PLUGIN_WRITE:
    {
      P126_data_struct *P126_data = static_cast<P126_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr == P126_data) {
        return success;
      }

      success = P126_data->plugin_write(event, string);

      break;
    }
  }
  return success;
}

#endif // ifdef USES_P126
