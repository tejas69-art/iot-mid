// Define the GPIO pin connected to the voltage divider
#define BATTERY_PIN 34  // Change this if using a different GPIO pin for ADC

// Define the voltage divider ratio (for 10kΩ/10kΩ divider, it's 2)
const float VOLTAGE_DIVIDER_RATIO = 2.0;
// ADC reference voltage for the ESP32
const float ADC_REF_VOLTAGE = 3.3;
// Maximum ADC value for the ESP32 (12-bit ADC, 0 to 4095)
const int ADC_MAX = 4095;

// Battery voltage range (in volts)
const float MIN_BATTERY_VOLTAGE = 3.0; // Considered 0% charge
const float MAX_BATTERY_VOLTAGE = 4.2; // Considered 100% charge

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);  // Set the ADC resolution to 12 bits
}

void loop() {
  // Read the raw ADC value
  int adcValue = analogRead(BATTERY_PIN);

  // Convert the ADC value to actual battery voltage
  float measuredVoltage = (adcValue / float(ADC_MAX)) * ADC_REF_VOLTAGE * VOLTAGE_DIVIDER_RATIO;

  // Map the voltage to battery percentage
  float batteryPercentage = ((measuredVoltage - MIN_BATTERY_VOLTAGE) / 
                             (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE)) * 100.0;

  // Constrain the percentage to be between 0 and 100
  batteryPercentage = constrain(batteryPercentage, 0, 100);

  // Display the battery percentage on the Serial Monitor
  Serial.print("Battery Voltage: ");
  Serial.print(measuredVoltage);
  Serial.print(" V, Charging Percentage: ");
  Serial.print(batteryPercentage);
  Serial.println(" %");

  delay(1000);  // Update every second
}
