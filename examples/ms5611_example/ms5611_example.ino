
#include <ms5611.h>

static ms5611 m_ms5611;

void setup(void) {
  Serial.begin(9600);

  Serial.println("==== TE Connectivity ====");
  Serial.println("======== MS5611 =========");
  Serial.println("======== Measure ========");

  m_ms5611.begin();
}
void loop(void) {
  ms5611_status status;
  float temperature;
  float pressure;
  boolean connected;

  connected = m_ms5611.is_connected();
  if (connected) {
    Serial.println("");
    status = m_ms5611.read_temperature_and_pressure(&temperature, &pressure);

    Serial.print("---Temperature = ");
    Serial.print(temperature, 1);
    Serial.print((char)176);
    Serial.println("C");

    Serial.print("---Pressure = ");
    Serial.print(pressure, 1);
    Serial.println("hPa");
  } else {
    Serial.println("Sensor Disconnected");
  }

  delay(1000);
}