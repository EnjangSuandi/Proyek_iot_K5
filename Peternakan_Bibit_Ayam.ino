#include "arduino_secrets.h"
#include "thingProperties.h" // File dari Arduino IoT Cloud
#include <Servo.h>
#include "DHT.h"

// --- Konfigurasi Perangkat Keras ---
#define DHT_TYPE DHT11
#define DHT_PIN D1   // Pin data DHT11
#define SERVO_PIN D4 // Pin sinyal Servo
#define RELAY_PIN D2 // Pin sinyal Relay

// --- Inisialisasi Library ---
DHT dht(DHT_PIN, DHT_TYPE);
Servo myservo;

// --- Variabel Timer Non-Blocking ---
unsigned long lastSensorReadTime = 0;
unsigned long lastServoTime = 0;
const long sensorReadInterval = 2000; // Baca sensor setiap 2 detik
const long servoDebounceTime = 1000;  // Jeda anti-spam untuk servo 1 detik

// --- Asumsi Status Relay ---
// Sebagian besar modul relay "Active LOW" (LOW = Nyala, HIGH = Mati)
// Ubah ini jika relay Anda "Active HIGH"
#define RELAY_ON LOW
#define RELAY_OFF HIGH

void setup() {
  // Mulai komunikasi serial untuk debugging
  Serial.begin(115200);
  delay(1500); // Beri waktu untuk serial monitor terhubung

  Serial.println("Mulai Setup Proyek Pakan Ayam...");

  dht.begin(); // Mulai sensor DHT
  
  myservo.attach(SERVO_PIN);
  myservo.write(0); // Pastikan servo di posisi tertutup saat awal
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF); // Pastikan lampu mati saat awal
  
  // Inisialisasi variabel di Arduino IoT Cloud
  initProperties();

  // Hubungkan ke Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  
  setDebugMessageLevel(2); // Tampilkan info debug
  ArduinoCloud.printDebugInfo();
  Serial.println("Setup Selesai. Menunggu koneksi cloud...");
}

void loop() {
  ArduinoCloud.update(); // Ini PENTING! Jaga koneksi ke cloud

  unsigned long currentTime = millis();

  // --- 1. Logika Sensor & Kontrol Lampu (Non-Blocking) ---
  if (currentTime - lastSensorReadTime >= sensorReadInterval) {
    lastSensorReadTime = currentTime; // Reset timer sensor

    // Baca data dari sensor DHT
    temperature = dht.readTemperature(); // Variabel 'temperature' dari cloud
    humidity = dht.readHumidity();     // Variabel 'humidity' dari cloud

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Gagal membaca data dari sensor DHT!");
    } else {
      // Cetak nilai ke Serial Monitor untuk debugging
      Serial.print("Suhu: ");
      Serial.print(temperature);
      Serial.print("°C, Kelembapan: ");
      Serial.print(humidity);
      Serial.println("%");

      // --- LOGIKA KONTROL LAMPU (HISTERESIS) ---
      // Logika ini sekarang ada di loop(), bukan di callback.
      // Ini adalah cara yang benar untuk kontrol otomatis.
      
      if (temperature < 28.0) {
        // Suhu dingin, nyalakan lampu
        digitalWrite(RELAY_PIN, RELAY_ON);
        if (relay == false) { // 'relay' adalah variabel bool di cloud
          relay = true; // Update status di cloud
          Serial.println("Status: Lampu ON (Suhu < 28°C)");
        }
      } else if (temperature > 30.0) {
        // Suhu panas, matikan lampu
        digitalWrite(RELAY_PIN, RELAY_OFF);
        if (relay == true) {
          relay = false; // Update status di cloud
          Serial.println("Status: Lampu OFF (Suhu > 30°C)");
        }
      }
      // Jika suhu di antara 28°C dan 30°C, status lampu tidak berubah (histeresis)
      // Ini mencegah relay menyala-mati terus-menerus.
    }
  }
}

// --- 2. Logika Kontrol Servo (Dipanggil oleh Callback) ---

// Fungsi helper ini dipanggil setiap kali saklar manual ATAU timer berubah
void updateServoState() {
  // Gunakan timer non-blocking untuk mencegah servo "spam" atau "jitter"
  if (millis() - lastServoTime < servoDebounceTime) {
    return; // Abaikan jika perubahan terlalu cepat
  }
  lastServoTime = millis();

  // Logika: Servo aktif jika (Timer terjadwal AKTIF) ATAU (Saklar manual 'servo' ON)
  if (timer.isActive() || servo == true) {
    Serial.println("Servo: ON (Posisi 90° - Memberi Pakan)");
    myservo.write(90); // Buka pakan (ubah sudut sesuai kebutuhan)
  } else {
    Serial.println("Servo: OFF (Posisi 0° - Tutup)");
    myservo.write(0); // Tutup pakan
  }
}

// --- Callback Variabel Cloud ---

// Dipanggil saat saklar 'servo' (manual) di dashboard berubah
void onServoChange() {
  Serial.println("-> Saklar 'servo' manual diubah.");
  updateServoState();
}

// Dipanggil saat 'timer' (jadwal) di dashboard berubah status (aktif/tidak aktif)
void onTimerChange() {
  Serial.println("-> Status 'timer' (jadwal) berubah.");
  updateServoState();
}

// Callback ini hanya untuk logging, tidak ada logika kontrol di sini.
void onTemperatureChange() {
  // Fungsi ini dipanggil SETELAH 'temperature' di loop() di-update ke cloud
  Serial.print("Cloud menerima update Suhu: ");
  Serial.println(temperature);
}

void onHumidityChange() {
  // Fungsi ini dipanggil SETELAH 'humidity' di loop() di-update ke cloud
  Serial.print("Cloud menerima update Kelembapan: ");
  Serial.println(humidity);
}

// Callback ini hanya untuk logging, tidak ada logika kontrol di sini.
void onRelayChange() {
  // Fungsi ini dipanggil SETELAH 'relay' di loop() di-update ke cloud
  Serial.print("Cloud menerima update status Relay: ");
  Serial.println(relay ? "ON" : "OFF");
}
