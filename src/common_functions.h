#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "DHTesp.h"

/// Expands %key% placeholders in `text` from `values`.
///
/// Scans the text once and looks up only the placeholders it actually contains.
/// (Doing it the other way round — looping over every key in the document and
/// calling String::replace for each — costs ~3 heap allocations per key on
/// every URL, payload and log line, which is expensive on a ~20 KB heap.)
/// An unknown or unterminated placeholder is left exactly as written.
/// (JsonObject is a lightweight handle, so it is taken by value as ArduinoJson
/// intends — a const reference makes its accessors unavailable.)
inline String expandPlaceholders(const String &text, JsonObject values) {
  if (text.indexOf('%') < 0) {
    return text; // nothing to expand
  }
  String out;
  out.reserve(text.length());

  unsigned int pos = 0;
  while (pos < text.length()) {
    const int start = text.indexOf('%', pos);
    if (start < 0) {
      out += text.substring(pos);
      break;
    }
    const int end = text.indexOf('%', start + 1);
    if (end < 0) {
      out += text.substring(pos); // unterminated, copy the rest verbatim
      break;
    }
    out += text.substring(pos, start);
    const String key = text.substring(start + 1, end);
    // Look the key up, and read it back, as a C string: ArduinoJson only knows
    // Arduino's String on-device, so going through const char* keeps this
    // helper host-testable. Every value in sensorData is stored as a string
    // (see TeHyBug::addSensorData), so nothing is lost.
    if (values.containsKey(key.c_str())) {
      const char *value = values[key.c_str()].as<const char *>();
      if (value != nullptr) {
        out += value;
      }
    } else {
      out += text.substring(start, end + 1); // leave it as written
    }
    pos = end + 1;
  }
  return out;
}

/// <summary>
/// Adds a leading 0 to a number if it is smaller than 10
/// </summary>
String IntFormat(int _int) {
  if (_int < 10) {
    return "0" + String(_int);
  }
  return String(_int);
}

/// <summary>
/// Convert RSSI to percentage quality
/// </summary>
int GetRSSIasQuality(int rssi) {
  int quality = 0;

  if (rssi <= -100) {
    quality = 0;
  } else if (rssi >= -50) {
    quality = 100;
  } else {
    quality = 2 * (rssi + 100);
  }
  return quality;
}

float temp2Imp(const float & value) {
  return (1.8 * value + 32);
}

// IO scenarios are encoded as "io<pin><level>", e.g. "io121" = pin 12 high
bool isIoScenario(const String &type) {
  return type.substring(0, 2) == "io";
}
uint8_t ioScenarioPin(const String &type) {
  return atoi(type.substring(2, 4).c_str());
}
uint8_t ioScenarioLevel(const String &type) {
  size_t lenz = type.length();
  return atoi(type.substring(lenz - 1, lenz).c_str());
}

String key2unit(const String & key)
{
  if (key == "temp" || key == "temp2" || key == "dew" || key == "hi")
    return "°C";
  else if (key == "temp_imp" || key == "temp2_imp" || key == "dew_imp" || key == "hi_imp")
    return "°F";
  else if (key == "humi")
    return "%RH";
  else if (key == "ah")
    return "g/m³";
  else if (key == "air")
    return "kOhm";
  else if (key == "qfe")
    return "hPa";
  else if (key == "alt")
    return "m";
  else if (key == "lux")
    return "Lux";
  else if (key == "adc")
    return "ADC";
  else if (key == "co2")
    return "ppm";
  else if (key == "cr")
    return "%";

  return "";
}
String cf2name(int cs)
{
  switch(cs) {
    case Comfort_OK:
      return "OK";
    case Comfort_TooHot:
      return "Too hot";
    case Comfort_TooCold:
      return "Too cold";
    case Comfort_TooDry:
      return "Too dry";
    case Comfort_TooHumid:
      return "Too humid";
    case Comfort_HotAndHumid:
      return "Hot and humid";
    case Comfort_HotAndDry:
      return "Hot and dry";
    case Comfort_ColdAndHumid:
      return "Cold and humid";
    case Comfort_ColdAndDry:
      return "Cold and dry";
    default:
      return "Unknown";
  };
}
String key2name(const String & key)
{
  if (key == "temp" || key == "temp_imp")
    return "Temperature";
  else if (key == "temp2" || key == "temp2_imp")
    return "Temperature2";
  else if (key == "humi")
    return "Humidity";
  else if (key == "ah")
    return "Absolute humidity";
  else if (key == "cr")
    return "Comfort ratio";
  else if (key == "cs")
    return "Comfort status";
  else if (key == "dew" || key == "dew_imp")
    return "Dew point";
  else if (key == "hi" || key == "hi_imp")
    return "Heat index";
  else if (key == "air")
    return "Gas resistance";
  else if (key == "iaq")
    return "Indoor air quality";
  else if (key == "qfe")
    return "Atmospheric pressure";
  else if (key == "alt")
    return "Altitude";
  else if (key == "eco2")
    return "CO2 equivalent";
  else if (key == "co2")
    return "CO2";
  else if (key == "bvoc")
    return "breath VOC equivalent";
  else if (key == "uv")
    return "UV index";
  else if (key == "lux")
    return "Ambient light";
  else if (key == "adc")
    return "ADC";

  return "";
}
String key2icon(const String & key)
{
  if (key == "temp" || key == "temp2" || key == "temp_imp" || key == "temp2_imp")
    return "mdi:thermometer";
  else if (key == "humi")
    return "mdi:water-percent";
  else if (key == "ah")
    return "mdi:water";
  else if (key == "cr" || key == "cs")
    return "mdi:sofa-outline";
  else if (key == "qfe")
    return "mdi:gauge";
  else if (key == "alt")
    return "mdi:image-filter-hdr";
  else if (key == "dew" || key == "dew_imp")
    return "mdi:water-thermometer";
  else if (key == "hi" || key == "hi_imp")
    return "mdi:sun-thermometer";
  else if (key == "air")
    return "mdi:resistor";
  else if (key == "co2" || key == "eco2")
    return "mdi:molecule-co2";
  else if (key == "iaq")
    return "mdi:airballoon-outline";

  return "mdi:help";
}
