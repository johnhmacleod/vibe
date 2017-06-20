void gotoSleep(int sleepCount);

void mqttConnect() {
  Serial.println("ClientID: " + String(clientId));
  Serial.println("Authmethod: " + String(authMethod));
  Serial.println("Token: " + String(token));
  
 if (!!!client.connected()) {
   Serial.print("Reconnecting MQTT client to "); Serial.println(server);
   int i = 0; 
   while (i < 40 && !!!client.connect(clientId, authMethod, token)) {
      digitalWrite(D4, LOW);
      delay(50);
      digitalWrite(D4, HIGH);
      delay(100);
      digitalWrite(D4, LOW);
      delay(50);
      digitalWrite(D4, HIGH);
      Serial.print(".");
      delay(500);
      i++;
 }
 Serial.println("OK");
 }
 if (!!!client.connected())
  gotoSleep(4);
}

void callback(char* topic, byte* payload, unsigned int payloadLength) {
 Serial.print("callback invoked for topic: "); Serial.println(topic);
}


