
  void readSpiffs() {
  //clean FS, for testing
  // Serial.println("Formatting SPIFFS"); SPIFFS.format(); Serial.println("Done");//**********************************************

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(org, json["org"]);
          strcpy(deviceType, json["deviceType"]);
          strcpy(deviceId, json["deviceId"]);
          strcpy(token, json["token"]);
          strcpy(perHour, json["perHour"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  }

void localParams() {
            // Set up paremeters
          sprintf(server, "%s%s", org, IBMserver);
          sprintf(clientId, "d:%s:%s:%s", org, deviceType, deviceId);
          Serial.println("Server: " + String(server));
          Serial.println("Client ID: " + String(clientId));
          int ph = atoi(perHour);
          if (ph > 0 && ph < 1800)
             publishInterval = 3600.0 / ph * 1000;
          Serial.println(publishInterval);
}

void writeSpiffs() {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["org"] = org;
    json["deviceType"] = deviceType;
    json["deviceId"] = deviceId;
    json["token"] = token;
    json["perHour"] = perHour;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }
    sprintf(server, "%s.%s", org, IBMserver);
    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
}

