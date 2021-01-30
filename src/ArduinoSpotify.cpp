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

#include "ArduinoSpotify.h"

ArduinoSpotify::ArduinoSpotify(WiFiClient &client, char *bearerToken)
{
    _client = &client;
    _bearerToken = String("Bearer ") + bearerToken;
    _http = new HTTPClient();
    _http->setTimeout(SPOTIFY_TIMEOUT);
    _http->setConnectTimeout(SPOTIFY_TIMEOUT);
 }

ArduinoSpotify::ArduinoSpotify(WiFiClient &client, const char *clientId, const char *clientSecret, const char *refreshToken)
{
    _client = &client;
    _clientId = clientId;
    _clientSecret = clientSecret;
    _refreshToken = refreshToken;
    _http = new HTTPClient();
    _http->setTimeout(SPOTIFY_TIMEOUT);
    _http->setConnectTimeout(SPOTIFY_TIMEOUT);
}

int ArduinoSpotify::makeRequestWithBody(const char *type, const char *uri, const char *authorization, const char *body, const char *contentType, const char *host)
{
    if (!_http->begin(*_client, String(host), (uint16_t)SPOTIFY_PORT, String(uri), true))
    {
        Serial.println(F("Connection failed"));
        return -1;
    }

    _http->addHeader(F("Accept"), F("application/json"));
    _http->addHeader(F("Content-Type"), contentType);
    // Will be replaced by HttpClient, if > 0)
    _http->addHeader(F("Content-Length"), "0");

    if (authorization != NULL)
    {
        _http->addHeader(F("Authorization"), authorization);
    }

    // give the esp a breather
    yield();

    if(strcmp(type, "PUT") == 0) {
        return _http->PUT((uint8_t*)body, strlen(body));
    } else {
        return _http->POST((uint8_t*)body, strlen(body));
    }
}

int ArduinoSpotify::makePutRequest(const char *uri, const char *authorization, const char *body, const char *contentType, const char *host)
{
    return makeRequestWithBody("PUT", uri, authorization, body, contentType);
}

int ArduinoSpotify::makePostRequest(const char *uri, const char *authorization, const char *body, const char *contentType, const char *host)
{
    return makeRequestWithBody("POST", uri, authorization, body, contentType, host);
}

int ArduinoSpotify::makeGetRequest(const char *uri, const char *authorization, const char *accept, const char *host)
{
    if (!_http->begin(*_client, String(host), (uint16_t)SPOTIFY_PORT, String(uri), true))
    {
        Serial.println(F("Connection failed"));
        return -1;
    }

    if (accept != NULL)
    {
        _http->addHeader(F("Accept"), accept);
    }

    if (authorization != NULL)
    {
        _http->addHeader(F("Authorization"), authorization);
    }

    // give the esp a breather
    yield();

    return _http->GET();
}

void ArduinoSpotify::setRefreshToken(const char *refreshToken)
{
    _refreshToken = refreshToken;
}

bool ArduinoSpotify::refreshAccessToken()
{
    char body[1000];
    sprintf(body, refreshAccessTokensBody, _refreshToken, _clientId, _clientSecret);

#ifdef SPOTIFY_DEBUG
    Serial.println(body);
#endif

    int statusCode = makePostRequest(SPOTIFY_TOKEN_ENDPOINT, NULL, body, "application/x-www-form-urlencoded", SPOTIFY_ACCOUNTS_HOST);
    unsigned long now = millis();

#ifdef SPOTIFY_DEBUG
    Serial.print("status Code: ");
    Serial.println(statusCode);
#endif

    bool refreshed = false;
    if (statusCode == 200)
    {
        DynamicJsonDocument doc(1000);
        DeserializationError error = deserializeJson(doc, _http->getStream());
        if (!error)
        {
            _bearerToken = String("Bearer ") + doc["access_token"].as<String>();
            int tokenTtl = doc["expires_in"];             // Usually 3600 (1 hour)
            _tokenTimeToLiveMs = (tokenTtl * 1000) - 2000; // The 2000 is just to force the token expiry to check if its very close
            _timeTokenRefreshed = now;
            refreshed = true;
        }
    }
    else
    {
        if(statusCode > 0)
        {
            parseError();
        }
    }

    stopClient();
    return refreshed;
}

bool ArduinoSpotify::checkAndRefreshAccessToken()
{
    unsigned long timeSinceLastRefresh = millis() - _timeTokenRefreshed;
    if (timeSinceLastRefresh >= _tokenTimeToLiveMs)
    {
        Serial.println("Refresh of the Access token is due, doing that now.");
        return refreshAccessToken();
    }

    // Token is still valid
    return true;
}

const char *ArduinoSpotify::requestAccessTokens(const char *code, const char *redirectUrl)
{
    char body[1000];
    sprintf(body, requestAccessTokensBody, code, redirectUrl, _clientId, _clientSecret);

#ifdef SPOTIFY_DEBUG
    Serial.println(body);
#endif

    int statusCode = makePostRequest(SPOTIFY_TOKEN_ENDPOINT, NULL, body, "application/x-www-form-urlencoded", SPOTIFY_ACCOUNTS_HOST);
    unsigned long now = millis();

#ifdef SPOTIFY_DEBUG
    Serial.print("status Code");
    Serial.println(statusCode);
#endif

    if (statusCode == 200)
    {
        DynamicJsonDocument doc(1000);
        DeserializationError error = deserializeJson(doc, _http->getStream());
        if (!error)
        {
            this->_bearerToken = String("Bearer ") + doc["access_token"].as<String>();
            _refreshToken = doc["refresh_token"].as<char *>();
            int tokenTtl = doc["expires_in"];             // Usually 3600 (1 hour)
            _tokenTimeToLiveMs = (tokenTtl * 1000) - 2000; // The 2000 is just to force the token expiry to check if its very close
            _timeTokenRefreshed = now;
        }
    }
    else
    {
        if(statusCode > 0)
        {
            parseError();
        }
    }

    stopClient();
    return _refreshToken;
}

bool ArduinoSpotify::play(const char *deviceId)
{
    char command[100] = SPOTIFY_PLAY_ENDPOINT;
    return playerControl(command, deviceId);
}

bool ArduinoSpotify::playAdvanced(const char *body, const char *deviceId)
{
    char command[100] = SPOTIFY_PLAY_ENDPOINT;
    return playerControl(command, deviceId, body);
}

bool ArduinoSpotify::pause(const char *deviceId)
{
    char command[100] = SPOTIFY_PAUSE_ENDPOINT;
    return playerControl(command, deviceId);
}

bool ArduinoSpotify::setVolume(int volume, const char *deviceId)
{
    char command[125];
    sprintf(command, SPOTIFY_VOLUME_ENDPOINT, volume);
    return playerControl(command, deviceId);
}

bool ArduinoSpotify::toggleShuffle(bool shuffle, const char *deviceId)
{
    char command[125];
    char shuffleState[10];
    if (shuffle)
    {
        strcpy(shuffleState, "true");
    }
    else
    {
        strcpy(shuffleState, "false");
    }
    sprintf(command, SPOTIFY_SHUFFLE_ENDPOINT, shuffleState);
    return playerControl(command, deviceId);
}

bool ArduinoSpotify::setRepeatMode(RepeatOptions repeat, const char *deviceId)
{
    char command[125];
    char repeatState[10];
    switch (repeat)
    {
    case repeat_track:
        strcpy(repeatState, "track");
        break;
    case repeat_context:
        strcpy(repeatState, "context");
        break;
    case repeat_off:
        strcpy(repeatState, "off");
        break;
    }

    sprintf(command, SPOTIFY_REPEAT_ENDPOINT, repeatState);
    return playerControl(command, deviceId);
}

bool ArduinoSpotify::playerControl(char *command, const char *deviceId, const char *body)
{
    if (deviceId[0] != '\0')
    {
        char *questionMarkPointer;
        questionMarkPointer = strchr(command, '?');
        // DeviceId is char[41] so let's add 20 to be safe with the rest
        char deviceIdBuff[60];
        if (questionMarkPointer == NULL)
        {
            sprintf(deviceIdBuff, "?device_id=%s", deviceId);
        }
        else
        {
            // params already started
            sprintf(deviceIdBuff, "&device_id=%s", deviceId);
        }
        strcat(command, deviceIdBuff);
    }

#ifdef SPOTIFY_DEBUG
    Serial.println(command);
    Serial.println(body);
#endif

    if (autoTokenRefresh)
    {
        checkAndRefreshAccessToken();
    }

    int statusCode = makePutRequest(command, _bearerToken.c_str(), body);

    stopClient();

    //Will return 204 if all went well.
    return (statusCode == 204);
}

bool ArduinoSpotify::playerNavigate(char *command, const char *deviceId)
{
    if (deviceId[0] != 0)
    {
        // DeviceId is char[41]
        char deviceIdBuff[60];
        sprintf(deviceIdBuff, "?device_id=%s", deviceId);
        strcat(command, deviceIdBuff);
    }

#ifdef SPOTIFY_DEBUG
    Serial.println(command);
#endif

    if (autoTokenRefresh)
    {
        checkAndRefreshAccessToken();
    }
    int statusCode = makePostRequest(command, _bearerToken.c_str());

    stopClient();
    //Will return 204 if all went well.
    return statusCode == 204;
}

bool ArduinoSpotify::nextTrack(const char *deviceId)
{
    char command[100] = SPOTIFY_NEXT_TRACK_ENDPOINT;
    return playerNavigate(command, deviceId);
}

bool ArduinoSpotify::previousTrack(const char *deviceId)
{
    char command[100] = SPOTIFY_PREVIOUS_TRACK_ENDPOINT;
    return playerNavigate(command, deviceId);
}

bool ArduinoSpotify::seek(int position, const char *deviceId)
{
    char command[100] = SPOTIFY_SEEK_ENDPOINT;
    char tempBuff[100];
    sprintf(tempBuff, "?position_ms=%d", position);
    strcat(command, tempBuff);
    if (deviceId[0] != 0)
    {
        sprintf(tempBuff, "?device_id=%s", deviceId);
        strcat(command, tempBuff);
    }

#ifdef SPOTIFY_DEBUG
    Serial.println(command);
#endif

    if (autoTokenRefresh)
    {
        checkAndRefreshAccessToken();
    }
    int statusCode = makePutRequest(command, _bearerToken.c_str());
    stopClient();
    //Will return 204 if all went well.
    return statusCode == 204;
}

uint8_t ArduinoSpotify::getDevices(SpotifyDevice resultDevices[], uint8_t maxDevices)
{
#ifdef SPOTIFY_DEBUG
    Serial.println(SPOTIFY_DEVICES_ENDPOINT);
#endif

    if (autoTokenRefresh)
    {
        checkAndRefreshAccessToken();
    }

    int statusCode = makeGetRequest(SPOTIFY_DEVICES_ENDPOINT, _bearerToken.c_str());

    uint8_t results = 0;

    if (statusCode == 200)
    {
        // Get from https://arduinojson.org/v6/assistant/
        const size_t bufferSize = deviceBufferSize;

        // Allocate DynamicJsonDocument
        DynamicJsonDocument doc(bufferSize);

        // Parse JSON object
        DeserializationError error = deserializeJson(doc, _http->getStream());
        if (!error)
        {
            JsonArray devices = doc["devices"].as<JsonArray>();

            results = devices.size();

            if(results > maxDevices)
            {
                Serial.printf("Too many devices: %d > %d (ignoring some)\n", results, maxDevices);
                results = maxDevices;
            }

            for(uint8_t i = 0; i < results; i++)
            {
                resultDevices[i].id = devices[i]["id"].as<String>();
                resultDevices[i].isActive = devices[i]["is_active"].as<bool>();
                resultDevices[i].isPrivateSession = devices[i]["is_private_session"].as<bool>();
                resultDevices[i].isRestricted = devices[i]["is_restricted"].as<bool>();
                resultDevices[i].name = devices[i]["name"].as<String>();
                resultDevices[i].type = devices[i]["type"].as<String>();
                resultDevices[i].volumePrecent = devices[i]["volume_percent"].as<uint8_t>();
            }
        }
        else
        {
            Serial.print(F("deserializeJson() failed with code "));
            Serial.println(error.c_str());
        }
    } else
    {
        Serial.printf("Invalid HTTP response: %d\n", statusCode);
    }
    stopClient();
    return results;
}
bool ArduinoSpotify::transferPlayback(const char *deviceId, bool play)
{
#ifdef SPOTIFY_DEBUG
    Serial.println(SPOTIFY_TRANSFER_ENDPOINT);
#endif

    if (autoTokenRefresh)
    {
        checkAndRefreshAccessToken();
    }

    char body[100];
    sprintf(body, "{\"device_ids\":[\"%s\"],\"play\":\"%s\"}", deviceId, (play?"true":"false"));
    Serial.println(body);

    int statusCode = makePutRequest(SPOTIFY_TRANSFER_ENDPOINT, _bearerToken.c_str(), body);
    stopClient();
    //Will return 204 if all went well.
    return statusCode == 204;
}

CurrentlyPlaying ArduinoSpotify::getCurrentlyPlaying(const char *market)
{
    char command[100] = SPOTIFY_CURRENTLY_PLAYING_ENDPOINT;
    if (market[0] != 0)
    {
        char marketBuff[30];
        sprintf(marketBuff, "?market=%s", market);
        strcat(command, marketBuff);
    }

#ifdef SPOTIFY_DEBUG
    Serial.println(command);
#endif
    if (autoTokenRefresh)
    {
        checkAndRefreshAccessToken();
    }

    int statusCode = makeGetRequest(command, _bearerToken.c_str());

    CurrentlyPlaying currentlyPlaying;
    // This flag will get cleared if all goes well
    currentlyPlaying.error = true;

    if (statusCode == 200)
    {
        // Allocate DynamicJsonDocument
        DynamicJsonDocument doc(currentlyPlayingBufferSize);

        // Parse JSON object
        DeserializationError error = deserializeJson(doc, _http->getStream());
        if (!error)
        {
            currentlyPlaying.contextUri = doc["context"]["uri"].as<String>();

            JsonObject item = doc["item"];
            JsonObject firstArtist = item["album"]["artists"][0];

            currentlyPlaying.firstArtistName = firstArtist["name"].as<String>();
            currentlyPlaying.firstArtistUri = firstArtist["uri"].as<String>();

            currentlyPlaying.albumName = item["album"]["name"].as<String>();
            currentlyPlaying.albumUri = item["album"]["uri"].as<String>();

            JsonArray images = item["album"]["images"];

            // Images are returned in order of width, so last should be smallest.
            int numImages = images.size();
            int startingIndex = 0;
            if (numImages > SPOTIFY_NUM_ALBUM_IMAGES)
            {
                startingIndex = numImages - SPOTIFY_NUM_ALBUM_IMAGES;
                currentlyPlaying.numImages = SPOTIFY_NUM_ALBUM_IMAGES;
            }
            else
            {
                currentlyPlaying.numImages = numImages;
            }

            for (int i = 0; i < currentlyPlaying.numImages; i++)
            {
                int adjustedIndex = startingIndex + i;
                currentlyPlaying.albumImages[i].height = images[adjustedIndex]["height"].as<int>();
                currentlyPlaying.albumImages[i].width = images[adjustedIndex]["width"].as<int>();
                currentlyPlaying.albumImages[i].url = images[adjustedIndex]["url"].as<String>();
            }

            currentlyPlaying.trackName = item["name"].as<String>();
            currentlyPlaying.trackUri = item["uri"].as<String>();

            currentlyPlaying.isPlaying = doc["is_playing"].as<bool>();

            currentlyPlaying.progressMs = doc["progress_ms"].as<long>();
            currentlyPlaying.duraitonMs = item["duration_ms"].as<long>();

            currentlyPlaying.error = false;
        }
        else
        {
            Serial.print(F("deserializeJson() failed with code "));
            Serial.println(error.c_str());
        }
    }
    stopClient();
    return currentlyPlaying;
}

PlayerDetails ArduinoSpotify::getPlayerDetails(const char *market)
{
    char command[100] = SPOTIFY_PLAYER_ENDPOINT;
    if (market[0] != 0)
    {
        char marketBuff[30];
        sprintf(marketBuff, "?market=%s", market);
        strcat(command, marketBuff);
    }

#ifdef SPOTIFY_DEBUG
    Serial.println(command);
#endif

    // Get from https://arduinojson.org/v6/assistant/
    const size_t bufferSize = playerDetailsBufferSize;
    PlayerDetails playerDetails;
    // This flag will get cleared if all goes well
    playerDetails.error = true;
    if (autoTokenRefresh)
    {
        checkAndRefreshAccessToken();
    }

    int statusCode = makeGetRequest(command, _bearerToken.c_str());

    if (statusCode == 200)
    {
        // Allocate DynamicJsonDocument
        DynamicJsonDocument doc(bufferSize);

        // Parse JSON object
        DeserializationError error = deserializeJson(doc, _http->getStream());
        if (!error)
        {
            JsonObject device = doc["device"];
            
            playerDetails.device.id = device["id"].as<String>();
            playerDetails.device.name = device["name"].as<String>();
            playerDetails.device.type = device["type"].as<String>();
            playerDetails.device.isActive = device["is_active"].as<bool>();
            playerDetails.device.isPrivateSession = device["is_private_session"].as<bool>();
            playerDetails.device.isRestricted = device["is_restricted"].as<bool>();
            playerDetails.device.volumePrecent = device["volume_percent"].as<int>();

            playerDetails.progressMs = doc["progress_ms"].as<long>();
            playerDetails.isPlaying = doc["is_playing"].as<bool>();

            playerDetails.shuffleState = doc["shuffle_state"].as<bool>();

            const char *repeat_state = doc["repeat_state"]; // "off"

            if (strncmp(repeat_state, "track", 5) == 0)
            {
                playerDetails.repeateState = repeat_track;
            }
            else if (strncmp(repeat_state, "context", 7) == 0)
            {
                playerDetails.repeateState = repeat_context;
            }
            else
            {
                playerDetails.repeateState = repeat_off;
            }

            playerDetails.error = false;
        }
        else
        {
            Serial.print(F("deserializeJson() failed with code "));
            Serial.println(error.c_str());
        }
    }
    stopClient();
    return playerDetails;
}

bool ArduinoSpotify::getImage(char *imageUrl, Stream *file)
{
#ifdef SPOTIFY_DEBUG
    Serial.print(F("Parsing image URL: "));
    Serial.println(imageUrl);
#endif

    uint8_t lengthOfString = strlen(imageUrl);

    // We are going to just assume https, that's all I've
    // seen and I can't imagine a company will go back
    // to http

    if (strncmp(imageUrl, "https://", 8) != 0)
    {
        Serial.print(F("Url not in expected format: "));
        Serial.println(imageUrl);
        Serial.println("(expected it to start with \"https://\")");
        return false;
    }

    uint8_t protocolLength = 8;

    char *pathStart = strchr(imageUrl + protocolLength, '/');
    uint8_t pathIndex = pathStart - imageUrl;
    uint8_t pathLength = lengthOfString - pathIndex;
    char path[pathLength + 1];
    strncpy(path, pathStart, pathLength);
    path[pathLength] = '\0';

    uint8_t hostLength = pathIndex - protocolLength;
    char host[hostLength + 1];
    strncpy(host, imageUrl + protocolLength, hostLength);
    host[hostLength] = '\0';

#ifdef SPOTIFY_DEBUG

    Serial.print(F("host: "));
    Serial.println(host);

    Serial.print(F("len:host:"));
    Serial.println(hostLength);

    Serial.print(F("path: "));
    Serial.println(path);

    Serial.print(F("len:path: "));
    Serial.println(strlen(path));
#endif

    bool status = false;
    int statusCode = makeGetRequest(path, NULL, "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8", host);
#ifdef SPOTIFY_DEBUG
    Serial.print(F("statusCode: "));
    Serial.println(statusCode);
#endif
    if (statusCode == 200)
    {

#ifdef SPOTIFY_DEBUG
        Serial.print(F("file length: "));
        Serial.println(_http->getSize());
#endif

        _http->writeToStream(file);

#ifdef SPOTIFY_DEBUG
            Serial.println(F("Finished getting image"));
#endif

        status = true;
    }

    stopClient();

    return status;
}

void ArduinoSpotify::parseError()
{
    DynamicJsonDocument doc(1000);
    DeserializationError error = deserializeJson(doc, _http->getStream());
    if (!error)
    {
        Serial.println(F("getAuthToken error"));
        serializeJson(doc, Serial);
    }
    else
    {
        Serial.println(F("Could not parse error"));
    }
}

void ArduinoSpotify::stopClient()
{
#ifdef SPOTIFY_DEBUG
        Serial.println(F("Closing client"));
#endif
        _http->end();
}