#pragma once
#include <ESP8266HTTPClient.h>
#include "debug.h"

// Plain HTTP transport. Placeholder replacement (%temp% etc.) is the
// caller's responsibility so it happens exactly once per request.
namespace http {

String get(HTTPClient &http, WiFiClient &client, const String &url) {
  D_print("HTTP GET: ");
  D_println(url);
  http.begin(client, url); // Specify request destination
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  const int httpCode = http.GET(); // Send the request
  String payload{};
  D_println(httpCode);       // Print HTTP return code
  if (httpCode == HTTP_CODE_OK) {
    payload = http.getString(); // Get the response
    D_println(payload);
  }
  else if(httpCode < 0)
  {
    D_println(http.errorToString(httpCode));
  }
  http.end(); // Close connection
  return payload;
}

String post(HTTPClient &http, WiFiClient &client, const String &url, const String &body) {
  D_print("HTTP POST: ");
  D_println(url);
  http.begin(client, url); // Specify request destination
  http.addHeader("Content-Type", "application/json");

  const int httpCode = http.POST(body); // Send the request
  String payload{};
  D_println(httpCode);                  // Print HTTP return code
  if (httpCode == HTTP_CODE_OK) {
    payload = http.getString(); // Get the response
    D_println(payload);
  }
  else if(httpCode < 0)
  {
    D_println(http.errorToString(httpCode));
  }
  http.end(); // Close connection
  return payload;
}

} // namespace http
