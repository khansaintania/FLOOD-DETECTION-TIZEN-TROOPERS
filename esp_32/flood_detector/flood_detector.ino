#include <WiFi.h>
#include <HTTPClient.h>
#include <algorithm>

#define TRIG_PIN 5       
#define ECHO_PIN 18      
#define WATER_SENSOR 34  
#define LED_GREEN 25     +
#define LED_YELLOW 26    
#define LED_RED 27       
#define BUZZER 23        

// Konfigurasi jaringan & server
const char* WIFI_SSID = "KOST 38 A";
const char* WIFI_PASS = "Azalea54321";
const char* SERVER_URL = "http://192.168.1.50:5000/data";

// Sensor & logika
const int SAMPLE_INTERVAL_MS = 15000;
const int WARNING_THRESHOLD_PERCENT = 60;
const int DANGER_THRESHOLD_PERCENT = 85;

// KALIBRASI ULTRASONIC - SESUAIKAN INI!
const float SENSOR_MOUNT_HEIGHT_CM = 250.0; // TINGGI pemasangan sensor dari dasar (dinaikkan dari 100 ke 250)
const float MIN_DISTANCE_CM = 2.0;          
const float MAX_DISTANCE_CM = 400.0;        

// KALIBRASI WATER LEVEL SENSOR - SESUAIKAN INI!
const int WATER_SENSOR_DRY = 2500;          
const int WATER_SENSOR_WET = 1200;          

// Filter settings
const int NUM_ULTRASONIC_SAMPLES = 5;
const int NUM_WATER_SAMPLES = 3;

// Variabel status
bool wifiConnected = false;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(WATER_SENSOR, INPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // Inisialisasi output
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  digitalWrite(BUZZER, LOW);

  Serial.println("\n=== Flood Detection System ===");
  Serial.println("Calibrating sensors...");
  
  // Test kalibrasi awal
  testSensorCalibration();
  
  // Koneksi WiFi
  connectWiFi();
}

void testSensorCalibration() {
  Serial.println("=== Sensor Calibration Test ===");
  
  // Test water level sensor
  int waterRaw = analogRead(WATER_SENSOR);
  Serial.printf("Water Sensor Raw: %d\n", waterRaw);
  Serial.printf("Water Sensor %%: %d\n", readWaterLevelPercent());
  
  // Test ultrasonic beberapa kali
  Serial.println("Ultrasonic samples:");
  for(int i = 0; i < 5; i++) {
    float ultrasonic = readUltrasonicCM();
    float water_height = SENSOR_MOUNT_HEIGHT_CM - ultrasonic;
    float percent = (water_height / SENSOR_MOUNT_HEIGHT_CM) * 100.0;
    Serial.printf("  Sample %d: %.2f cm -> Height: %.2f cm -> %.1f%%\n", 
                  i+1, ultrasonic, water_height, percent);
    delay(500);
  }
  
  Serial.println("=== End Calibration Test ===");
}

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500);
    Serial.print(".");
    tries++;
    digitalWrite(LED_RED, !digitalRead(LED_RED));
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
    
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_GREEN, HIGH);
      delay(200);
      digitalWrite(LED_GREEN, LOW);
      delay(200);
    }
  } else {
    wifiConnected = false;
    Serial.println("WiFi connection failed!");
    digitalWrite(LED_RED, HIGH);
  }
}

float readUltrasonicCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  
  if (duration == 0) {
    return -1.0;
  }
  
  float distance_cm = (duration / 2.0) * 0.0343;
  return distance_cm;
}

float readFilteredUltrasonic() {
  float samples[NUM_ULTRASONIC_SAMPLES];
  int validSamples = 0;
  
  for(int i = 0; i < NUM_ULTRASONIC_SAMPLES; i++) {
    float dist = readUltrasonicCM();
    
    if(dist >= MIN_DISTANCE_CM && dist <= MAX_DISTANCE_CM) {
      samples[validSamples++] = dist;
    }
    delay(50);
  }
  
  if(validSamples == 0) {
    Serial.println("Ultrasonic: No valid samples");
    return -1.0;
  }
  
  std::sort(samples, samples + validSamples);
  float median = samples[validSamples / 2];
  
  float sum = 0;
  int count = 0;
  for(int i = 0; i < validSamples; i++) {
    if(abs(samples[i] - median) < 50.0) {
      sum += samples[i];
      count++;
    }
  }
  
  if(count == 0) return -1.0;
  return sum / count;
}

int readWaterLevelPercent() {
  int sum = 0;
  
  for(int i = 0; i < NUM_WATER_SAMPLES; i++) {
    int raw = analogRead(WATER_SENSOR);
    sum += raw;
    delay(10);
  }
  
  int avgRaw = sum / NUM_WATER_SAMPLES;
  
  // PERBAIKI: Handle inverted logic dengan benar
  int percent = map(avgRaw, WATER_SENSOR_DRY, WATER_SENSOR_WET, 0, 100);
  percent = constrain(percent, 0, 100);
  
  return percent;
}

int calculateWaterLevel(float ultrasonic_cm, int waterLevelPercent) {
  bool ultrasonicValid = (ultrasonic_cm >= MIN_DISTANCE_CM && ultrasonic_cm <= MAX_DISTANCE_CM);
  bool waterLevelValid = (waterLevelPercent >= 0 && waterLevelPercent <= 100);
  
  // Jika kedua sensor tidak valid
  if(!ultrasonicValid && !waterLevelValid) {
    Serial.println("Both sensors failed!");
    return -1;
  }
  
  // Jika hanya ultrasonic yang valid
  if(ultrasonicValid && !waterLevelValid) {
    float water_height = SENSOR_MOUNT_HEIGHT_CM - ultrasonic_cm;
    water_height = constrain(water_height, 0, SENSOR_MOUNT_HEIGHT_CM);
    float percent = (water_height / SENSOR_MOUNT_HEIGHT_CM) * 100.0;
    Serial.printf("Using ultrasonic only: %.2f cm -> %.1f%%\n", ultrasonic_cm, percent);
    return constrain((int)percent, 0, 100);
  }
  
  // Jika hanya water level yang valid
  if(!ultrasonicValid && waterLevelValid) {
    Serial.println("Using water level only (ultrasonic invalid)");
    return waterLevelPercent;
  }
  
  // Kedua sensor valid - lakukan cross-check
  float water_height = SENSOR_MOUNT_HEIGHT_CM - ultrasonic_cm;
  water_height = constrain(water_height, 0, SENSOR_MOUNT_HEIGHT_CM);
  float ultrasonicPercent = (water_height / SENSOR_MOUNT_HEIGHT_CM) * 100.0;
  ultrasonicPercent = constrain(ultrasonicPercent, 0, 100);
  
  int diff = abs(ultrasonicPercent - waterLevelPercent);
  
  // DEBUG: Tampilkan detail perhitungan
  Serial.printf("DEBUG: Ultrasonic=%.2f cm, Height=%.2f cm, US%%=%.1f%%, WL%%=%d%%, Diff=%d%%\n", 
                ultrasonic_cm, water_height, ultrasonicPercent, waterLevelPercent, diff);
  
  // Jika perbedaan terlalu besar, prioritaskan ultrasonic
  if(diff > 40) {
    Serial.printf("Large sensor difference: Ultrasonic=%.1f%%, WaterLevel=%d%%. Trusting ultrasonic.\n", 
                  ultrasonicPercent, waterLevelPercent);
    return (int)ultrasonicPercent;
  }
  
  // Jika perbedaan kecil, gunakan weighted average
  int finalPercent = (ultrasonicPercent * 0.7 + waterLevelPercent * 0.3);
  Serial.printf("Sensor agreement: Ultrasonic=%.1f%%, WaterLevel=%d%%, Final=%d%%\n", 
                ultrasonicPercent, waterLevelPercent, finalPercent);
  
  return constrain(finalPercent, 0, 100);
}

void setOutputs(int percent) {
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  noTone(BUZZER);

  if (percent == -1) {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_YELLOW, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    delay(200);
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_GREEN, LOW);
    return;
  }

  if (percent >= DANGER_THRESHOLD_PERCENT) {
    digitalWrite(LED_RED, HIGH);
    tone(BUZZER, 2000, 500);
  } else if (percent >= WARNING_THRESHOLD_PERCENT) {
    digitalWrite(LED_YELLOW, HIGH);
    tone(BUZZER, 1500, 200);
  } else {
    digitalWrite(LED_GREEN, HIGH);
  }
}

bool sendData(float ultrasonic_cm, int waterLevelPercent, int finalPercent) {
  // Validasi data sebelum kirim - LEBIH KETAT
  if(finalPercent < 0 || finalPercent > 100) {
    Serial.printf("❌ Invalid finalPercent: %d - skipping send\n", finalPercent);
    return false;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting reconnect...");
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      return false;
    }
  }

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);

  String sensorStatus = "unknown";
  bool ultrasonicValid = (ultrasonic_cm >= MIN_DISTANCE_CM && ultrasonic_cm <= MAX_DISTANCE_CM);
  bool waterLevelValid = (waterLevelPercent >= 0 && waterLevelPercent <= 100);
  
  if(ultrasonicValid && waterLevelValid) {
    sensorStatus = "both";
  } else if(ultrasonicValid) {
    sensorStatus = "ultrasonic";
  } else if(waterLevelValid) {
    sensorStatus = "water_level";
  } else {
    sensorStatus = "failed";
  }

  String payload = "{";
  payload += "\"device_id\":\"esp32_01\",";
  payload += "\"timestamp_ms\":" + String(millis()) + ",";
  payload += "\"ultrasonic_cm\":" + String(ultrasonic_cm, 2) + ",";
  payload += "\"water_level_percent\":" + String(waterLevelPercent) + ",";
  payload += "\"final_level_percent\":" + String(finalPercent) + ",";
  payload += "\"sensor_status\":\"" + sensorStatus + "\"";
  payload += "}";

  Serial.printf("Payload: %s\n", payload.c_str());
  
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("✅ POST success: %d\n", httpResponseCode);
    Serial.printf("Response: %s\n", response.c_str());
    http.end();
    return true;
  } else {
    Serial.printf("❌ POST failed: %d\n", httpResponseCode);
    Serial.printf("Error: %s\n", http.errorToString(httpResponseCode).c_str());
    http.end();
    return false;
  }
}

void printSensorStatus(float ultrasonic_cm, int waterLevelPercent, int finalPercent) {
  Serial.println("=== Sensor Readings ===");
  Serial.printf("Ultrasonic: %.2f cm", ultrasonic_cm);
  if(ultrasonic_cm < MIN_DISTANCE_CM || ultrasonic_cm > MAX_DISTANCE_CM) {
    Serial.print(" [INVALID]");
  }
  Serial.println();
  
  Serial.printf("Water Level: %d%%", waterLevelPercent);
  if(waterLevelPercent < 0 || waterLevelPercent > 100) {
    Serial.print(" [INVALID]");
  }
  Serial.println();
  
  Serial.printf("Final Level: %d%%", finalPercent);
  if(finalPercent == -1) {
    Serial.print(" [SENSOR ERROR]");
  }
  Serial.println();
  Serial.println("=======================");
}

void loop() {
  unsigned long currentTime = millis();
  
  float ultrasonic_cm = readFilteredUltrasonic();
  int waterLevelPercent = readWaterLevelPercent();
  int finalPercent = calculateWaterLevel(ultrasonic_cm, waterLevelPercent);
  
  printSensorStatus(ultrasonic_cm, waterLevelPercent, finalPercent);
  setOutputs(finalPercent);
  
  if(finalPercent >= 0) {
    bool sendSuccess = sendData(ultrasonic_cm, waterLevelPercent, finalPercent);
    if(!sendSuccess) {
      Serial.println("Failed to send data to server");
    }
  }
  
  unsigned long elapsed = millis() - currentTime;
  if(elapsed < SAMPLE_INTERVAL_MS) {
    delay(SAMPLE_INTERVAL_MS - elapsed);
  } else {
    delay(1000);
  }
}