/*!
 *  @brief Configura y se conecta a la red Wi-Fi
 */
void setupWifi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a la red.");
  while (WiFi.status() != WL_CONNECTED){  
    delay(500);
    Serial.print(".") ;
  }
  Serial.println(".") ;
  Serial.print("Conectado a la red ");
  Serial.println(ssid);
  Serial.print("Dirección IP:\t");
  Serial.println(WiFi.localIP());
}

WiFiClient espClient;
PubSubClient client(espClient);

/*!
 *  @brief Configura y se conecta a un servidor MQTT
 */
void setupMQTT(){
  client.setServer(mqttServer, mqttPort);
  while (!client.connected()){      
    Serial.println("Conectando al broker MQTT...");
    if (client.connect("ESP32Client"))
      Serial.println("Conectado al broker MQTT");
    else{
      Serial.print("Falló conexión al broker MQTT");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

/*!
 *  @brief Se reconecta al servidor en caso de perder la conexión
 */
void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando al broker MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Conectado al broker MQTT");
    } else {
      Serial.print("Falló conexión al broker MQTT");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(2000);
    }
  }
}

/*!
 *  @brief Publica un valor de punto flotante utilizando el protocolo MQTT
 *  @param  value
 *          Valor de punto flotante a publicar
 *  @param  topic
 *          Tópico en el que publicar el valor
 */
void publishFloat(float value, const char*  topic){
  char valueString[8];
  dtostrf(value, 1, 2, valueString);
  client.publish(topic, valueString);
}

/*!
 *  @brief Publica una cadena de caracteres utilizando el protocolo MQTT
 *  @param  value
 *          Cadena de caracteres a publicar
 *  @param  topic
 *          Tópico en el que publicar la cadena de caracteres
 */
void publishString(const char* value, const char*  topic){
  client.publish(topic, value);
}