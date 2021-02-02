/*
ArduinoSpotify - An Arduino library to wrap the Spotify API

Copyright (c) 2020  Brian Lough.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef ArduinoSpotify_h
#define ArduinoSpotify_h

// I find setting these types of flags unreliable from the Arduino IDE
// so uncomment this if its not working for you.
// NOTE: Do not use this option on live-streams, it will reveal your
// private tokens!

//#define SPOTIFY_DEBUG 1

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#if defined(ESP32)
#include <HTTPClient.h>
#elif defined(ESP8266)
#include <ESP8266HTTPClient.h>
#endif

#define SPOTIFY_HOST "api.spotify.com"
#define SPOTIFY_ACCOUNTS_HOST "accounts.spotify.com"
#define SPOTIFY_PORT 443
// Fingerprint correct as of July 23rd, 2020
#define SPOTIFY_FINGERPRINT "B9 79 6B CE FD 61 21 97 A7 02 90 EE DA CD F0 A0 44 13 0E EB"
#define SPOTIFY_TIMEOUT 2000

#define SPOTIFY_CURRENTLY_PLAYING_ENDPOINT "/v1/me/player/currently-playing"

#define SPOTIFY_PLAYER_ENDPOINT "/v1/me/player"

#define SPOTIFY_PLAY_ENDPOINT "/v1/me/player/play"
#define SPOTIFY_PAUSE_ENDPOINT "/v1/me/player/pause"
#define SPOTIFY_VOLUME_ENDPOINT "/v1/me/player/volume?volume_percent=%d"
#define SPOTIFY_SHUFFLE_ENDPOINT "/v1/me/player/shuffle?state=%s"
#define SPOTIFY_REPEAT_ENDPOINT "/v1/me/player/repeat?state=%s"
#define SPOTIFY_TRANSFER_ENDPOINT "/v1/me/player"
#define SPOTIFY_DEVICES_ENDPOINT "/v1/me/player/devices"

#define SPOTIFY_NEXT_TRACK_ENDPOINT "/v1/me/player/next"
#define SPOTIFY_PREVIOUS_TRACK_ENDPOINT "/v1/me/player/previous"

#define SPOTIFY_SEEK_ENDPOINT "/v1/me/player/seek"

#define SPOTIFY_TOKEN_ENDPOINT "/api/token"

#define SPOTIFY_NUM_ALBUM_IMAGES 3

enum RepeatOptions
{
  REPEAT_TRACK,
  REPEAT_CONTEXT,
  REPEAT_OFF
};

struct SpotifyImage
{
  int height;
  int width;
  String url;
};

struct SpotifyDevice
{
  String id;
  String name;
  String type;
  bool isActive;
  bool isRestricted;
  bool isPrivateSession;
  uint8_t volumePrecent;
};

struct PlayerDetails
{
  SpotifyDevice device;

  long progressMs;
  bool isPlaying;
  RepeatOptions repeateState;
  bool shuffleState;

  bool error;
};

struct CurrentlyPlaying
{
  String firstArtistName;
  String firstArtistUri;
  String albumName;
  String albumUri;
  String trackName;
  String trackUri;
  String contextUri;
  SpotifyImage albumImages[SPOTIFY_NUM_ALBUM_IMAGES];
  int numImages;
  bool isPlaying;
  long progressMs;
  long duraitonMs;

  bool error;
};

class ArduinoSpotify
{
public:
  ArduinoSpotify(WiFiClient &client, char *bearerToken);
  ArduinoSpotify(WiFiClient &client, const char *clientId, const char *clientSecret, const char *refreshToken = "");

  // Auth Methods
  void setRefreshToken(const char *refreshToken);
  bool refreshAccessToken();
  bool checkAndRefreshAccessToken();
  const char *requestAccessTokens(const char *code, const char *redirectUrl);

  // Generic Request Methods
  int makeGetRequest(const char *command, const char *authorization, const char *accept = "application/json", const char *host = SPOTIFY_HOST);
  int makeRequestWithBody(const char *type, const char *command, const char *authorization, const char *body = "", const char *contentType = "application/json", const char *host = SPOTIFY_HOST);
  int makePostRequest(const char *command, const char *authorization, const char *body = "", const char *contentType = "application/json", const char *host = SPOTIFY_HOST);
  int makePutRequest(const char *command, const char *authorization, const char *body = "", const char *contentType = "application/json", const char *host = SPOTIFY_HOST);

  // User methods
  CurrentlyPlaying getCurrentlyPlaying(const char *market = "");
  PlayerDetails getPlayerDetails(const char *market = "");
  bool play(const char *deviceId = "");
  bool playAdvanced(const char *body, const char *deviceId = "");
  bool pause(const char *deviceId = "");
  bool setVolume(int volume, const char *deviceId = "");
  bool toggleShuffle(bool shuffle, const char *deviceId = "");
  bool setRepeatMode(RepeatOptions repeat, const char *deviceId = "");
  bool nextTrack(const char *deviceId = "");
  bool previousTrack(const char *deviceId = "");
  bool playerControl(char *command, const char *deviceId = "", const char *body = "");
  bool playerNavigate(char *command, const char *deviceId = "");
  bool seek(int position, const char *deviceId = "");
  uint8_t getDevices(SpotifyDevice devices[], uint8_t maxDevices);
  bool transferPlayback(const char *deviceId, bool play = false);

  // Image methods
  bool getImage(char *imageUrl, Stream *file);

  int tagArraySize = 10;
  int deviceBufferSize = 10000;
  int currentlyPlayingBufferSize = 10000;
  int playerDetailsBufferSize = 10000;
  bool autoTokenRefresh = true;

private:
  String _bearerToken;
  const char *_refreshToken;
  const char *_clientId;
  const char *_clientSecret;
  unsigned int _timeTokenRefreshed;
  unsigned int _tokenTimeToLiveMs;
  WiFiClient *_client;
  HTTPClient *_http;
  // Should not be needed, but might be use to save some RAM between requests
  void stopClient();
  void parseError();
  const char *requestAccessTokensBody =
      R"(grant_type=authorization_code&code=%s&redirect_uri=%s&client_id=%s&client_secret=%s)";
  const char *refreshAccessTokensBody =
      R"(grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s)";
};

#endif