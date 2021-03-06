/*******************************************************************
    Get Refresh Token from spotify, this is needed for the other
    examples.

    Note: MDNS does not seem to work well on the ESP32 so we will have to use
    the IP address for the redirect URL

    Instrucitons:

    - Put in your Wifi details, Client ID, Client secret and flash to the board
    - Get the Ip Address from the serial monitor
    - Add the following to Redirect URI on your Spotify app "http://[ESP_IP]/callback/"
    e.g. "http://192.168.1.20/callback/" (don't forget the last "/")
    - Open browser to esp using the IP address
    - Click the link
    - The Refresh Token will be printed to screen, use this
      for SPOTIFY_REFRESH_TOKEN in other examples.


    Parts:
    ESP32 D1 Mini stlye Dev board* - http://s.click.aliexpress.com/e/C6ds4my

 *  * = Affilate

    If you find what I do usefuland would like to support me,
    please consider becoming a sponsor on Github
    https://github.com/sponsors/witnessmenow/


    Written by Brian Lough
    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/

// ----------------------------
// Standard Libraries
// ----------------------------

#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <ArduinoSpotify.h>
// Library for connecting to the Spotify API

// Install from Github
// https://github.com/witnessmenow/arduino-spotify-api

#include <ArduinoJson.h>
// Library used for parsing Json from the API responses

// Search for "Arduino Json" in the Arduino Library manager
// https://github.com/bblanchon/ArduinoJson

//------- Replace the following! ------

char ssid[] = "SSID";                           // your network SSID (name)
char password[] = "password";                   // your network password
char clientId[] = "56t4373258u3405u43u543";     // Your client ID of your spotify APP
char clientSecret[] = "56t4373258u3405u43u543"; // Your client Secret of your spotify APP (Do Not share this!)

char scope[] = "user-read-playback-state%20user-modify-playback-state";
char callbackURItemplate[] = "%s%s%s";
char callbackURIProtocol[] = "http%3A%2F%2F"; // "http://"
char callbackURIAddress[] = "%2Fcallback%2F"; // "/callback/"
char callbackURI[100];

//------- ---------------------- ------

// including a "spotify_server_cert" variable
// header is included as part of the ArduinoSpotify libary
#include <ArduinoSpotifyCert.h>

WebServer server(80);

WiFiClientSecure client;
ArduinoSpotify spotify(client, clientId, clientSecret);

const char *webpageTemplate =
    R"(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no" />
  </head>
  <body>
    <div>
     <a href="https://accounts.spotify.com/authorize?client_id=%s&response_type=code&redirect_uri=%s&scope=%s">spotify Auth</a>
    </div>
  </body>
</html>
)";

void handleRoot()
{
  char webpage[800];
  sprintf(webpage, webpageTemplate, clientId, callbackURI, scope);
  server.send(200, "text/html", webpage);
}

void handleCallback()
{
  String code = "";
  const char *refreshToken = NULL;
  for (uint8_t i = 0; i < server.args(); i++)
  {
    if (server.argName(i) == "code")
    {
      code = server.arg(i);
      refreshToken = spotify.requestAccessTokens(code.c_str(), callbackURI);
    }
  }

  if (refreshToken != NULL)
  {
    server.send(200, "text/plain", refreshToken);
  }
  else
  {
    server.send(404, "text/plain", "Failed to load token, check serial monitor");
  }
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  Serial.print(message);
  server.send(404, "text/plain", message);
}

void setup()
{

  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  IPAddress ipAddress = WiFi.localIP();
  Serial.println(ipAddress);

  if (MDNS.begin("arduino"))
  {
    Serial.println("MDNS responder started");
  }

  // If you want to enable some extra debugging
  // uncomment the "#define SPOTIFY_DEBUG" in ArduinoSpotify.h
  client.setCACert(spotify_server_cert);

  // Building up callback URL using IP address.
  sprintf(callbackURI, callbackURItemplate, callbackURIProtocol, ipAddress.toString().c_str(), callbackURIAddress);

  server.on("/", handleRoot);
  server.on("/callback/", handleCallback);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  server.handleClient();
}
