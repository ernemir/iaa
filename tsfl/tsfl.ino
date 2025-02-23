#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
// modelo (incluir antes que <eloquent_tinyml.h>)
#include "model.h"
// runtime específico para la placa
#include <tflm_esp32.h>
#include <eloquent_tinyml.h>
#include "netconfig.h"
#include "util.hpp"

// Conexiones Sensores
const int pinDS18B20 = 5;     
const int pinDTH = 4;

// Parametrización
// Rango de temperatura ambiente de operación
const float tempMin = -20.0; 
const float tempMax = 40.0;
// Rango de temperatura del dispositivo
const float tempDispMin = 10.0; 
const float tempDispMax = 80.0;
// Umbral entre temperatura predicha y temperatura real
const float umbral = 5.0;

// Inicilización sensores
OneWire oneWire(pinDS18B20);
DallasTemperature sensors(&oneWire);
DHT sensorTH (pinDTH, DHT22);

// configuraciones del modelo
#define ARENA_SIZE 2000 //memoria para la ejecución del modelo
#define TF_NUM_OPS 1

Eloquent::TF::Sequential<TF_NUM_OPS, ARENA_SIZE> tf;

void setup() {
  // Inicializa el monitor serial
  Serial.begin(115200);
  // Inicializa el sensor DS18B20
  sensors.begin();
  sensorTH.begin();
  // Inicializa la conexión a la red
  setupWifi();
  // Inicializa la conexión al broker MQTT
  setupMQTT();
  // Inicializa el modelo 
  tf.setNumInputs(2);
  tf.setNumOutputs(1);
  tf.resolver.AddFullyConnected();
  while (!tf.begin(model).isOk()) 
    Serial.println(tf.exception.toString());
}

/*!
 *  @brief Normaliza el valor de temperatura de acuerdo a la media y desvío estandar de los datos del modelo
 *  @param  temp
 *          Valor de la temperatura sin normalizar
 *	@return Temperatura normalizada
 */
float normalizarTemp(float temp){
  float mean  = 24.624306;
  float std   = 4.629324;
  return (temp - mean) / std;
}

/*!
 *  @brief Normaliza el valor de humedad de acuerdo a la media y desvío estandar de los datos del modelo
 *  @param  temp
 *          Valor de la humedad sin normalizar
 *	@return Humedad normalizada
 */
float normalizarHum(float hum){
  float mean  = 70.460069;
  float std   = 18.515943;
  return (hum - mean) / std;
}

void loop() {
  // Tiempo de demora inferencia
  unsigned long T1 = 0;
  unsigned long T2 = 0;
  float tempPredict = 0.0;
  // Si se desconectó del broker
  if (!client.connected()) {
    // intentamos una reconexión
    reconnect();
  }
  client.loop();
  // Lectura de sensores ambientales
  float humAmbiente = sensorTH.readHumidity();
  float tempAmbiente = sensorTH.readTemperature();
  // Lectura del sensor del dispositivo
  sensors.requestTemperatures(); 
  float tempDispositivo = sensors.getTempCByIndex(0);
  // Publica lecturas de sensores
  publishFloat(tempAmbiente, "esp32/tempAmbiente");
  publishFloat(humAmbiente, "esp32/humAmbiente");
  publishFloat(tempDispositivo, "esp32/tempDispositivo");
  Serial.printf(
          "Temperatura = %.2f ºC Humedad = %.2f %% Dispositivo = %.2f ºC ", 
          tempAmbiente, 
          humAmbiente,
          tempDispositivo
      );
  // Verifica que este operando dentro de las condicones de operación 
  if ( tempAmbiente > tempMin && tempAmbiente < tempMax){
    // Predicción de la temperatura del dispositivo
    float input[2] = {normalizarTemp(tempAmbiente), normalizarHum(humAmbiente)};
    T1 = micros();
    while (!tf.predict(input).isOk())
      Serial.println(tf.exception.toString());
    T2 = micros();
    tempPredict = tf.output(0);
    Serial.printf(
          "Predicción %.2f ºC \n", 
          tempPredict
      );
    Serial.printf("(Tiempo de inferencia = %d microseg.) \n", T2-T1);  
    // verifica que la temperatura del dispositivo se encuentra dentro del rango recomendado
    if ( tempDispositivo > tempDispMin && tempDispositivo < tempDispMax){
        // verifica que la diferencia entre la temp del dispositivo y lo predicho no supere el umbral
        if ( abs(tempPredict - tempDispositivo) > umbral){
          // supera umbral --> enviamos alerta de verificar
          publishString("VERIFY", "esp32/alerta");
          Serial.println("ALERTA: Verificar funcionamiento!!");
        }
    } else {
      // temperatura del dispositivo fuera del rango recomendado
      // envia alerta de detener
      publishString("STOP", "esp32/alerta");
      Serial.println("ALERTA: Detener dispositivo!!");
    }
  } else {
    // temperatura ambiente fuera de rango recomendado
    // envia alerta de detener
    publishString("STOP", "esp32/alerta");
    Serial.println("ALERTA: Detener dispositivo!!");
  }
  // Espera 5 segundos entre medición y medición
  delay(5000);
}
