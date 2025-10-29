# 🌊 Proyek Deteksi Banjir IoT (ESP32)



Proyek ini adalah sistem pemantauan ketinggian air real-time berbasis **Internet of Things (IoT)** menggunakan mikrokontroler **ESP32** dan sensor ultrasonik. Data ketinggian air (`Level %` dan `Ultrasonic cm`) dikirim secara periodik ke *server* dan divisualisasikan melalui *dashboard* web.

---

## ✨ Fitur Utama

* **Pemantauan Real-Time:** Pengukuran ketinggian air secara *live* menggunakan sensor ultrasonik.
* **Transmisi Data:** Pengiriman data ke *server* melalui koneksi WiFi dan protokol HTTP POST.
* **Visualisasi Data:** *Dashboard* web yang menampilkan ringkasan, log data, dan grafik fluktuasi ketinggian air.
* **Deteksi Dini:** Penggunaan persentase level (`Level %`) untuk indikasi siaga/bahaya banjir.

---

## 🛠️ Persiapan

### 1. Komponen Hardware

* 1x **ESP32** Development Board (atau ESP8266).
* 1x Sensor **Ultrasonik** (HC-SR04 atau JSN-SR04T)
* Kabel Jumper
* Adaptor Daya 5V

### 2. Kebutuhan Software

* **IDE:** Arduino IDE atau Visual Studio Code dengan PlatformIO.
* **Database:** MySQL/MariaDB.
* **Web Server:** Apache/Nginx (untuk hosting dashboard).

### 3. Library Arduino yang Dibutuhkan

Instal library berikut melalui Library Manager di Arduino IDE:

* **`WiFi.h`** (Biasanya sudah termasuk)
* **`HTTPClient.h`** (Biasanya sudah termasuk)
* **`NewPing`** (Jika menggunakan HC-SR04/JSN-SR04T)

---

## 🔌 Diagram Pengkabelan (Wiring)

Hubungkan sensor ultrasonik (HC-SR04) ke ESP32 sebagai berikut:

| Sensor Pin | ESP32 Pin | Deskripsi |
| :---: | :---: | :--- |
| **VCC** | 5V | Catu daya. |
| **GND** | GND | Ground. |
| **Trig** | **GPIO 23** | Pin Pemicu. |
| **Echo** | **GPIO 22** | Pin Pantulan. |

>

---

## ⚙️ Cara Menjalankan Sistem

### Langkah 1: Setup Backend & Database

1.  **Database:** Buat database bernama `db_banjir` (contoh) dan buat tabel bernama `log_data` dengan kolom-kolom berikut:
    * `id` (INT, Primary Key, Auto Increment)
    * `device` (VARCHAR)
    * `waktu` (DATETIME)
    * `level_persen` (FLOAT)
    * `ultrasonic_cm` (FLOAT)

2.  **Skrip Server:** Letakkan skrip PHP/Python untuk menerima data (misalnya, `insert_data.php`) di *root* server web Anda. Skrip ini harus memproses *request* HTTP POST dan menyimpan data ke tabel `log_data`.

### Langkah 2: Konfigurasi Firmware ESP32

1.  Buka file sketsa Arduino Anda.
2.  Sesuaikan kredensial jaringan dan alamat API *server* pada baris-baris berikut:

    ```cpp
    // 📡 Konfigurasi WiFi
    const char* ssid = "NAMA_WIFI_ANDA";
    const char* password = "PASSWORD_WIFI_ANDA";

    // 🌐 Konfigurasi Server API
    const char* serverName = "[http://alamat.server.anda/insert_data.php](http://alamat.server.anda/insert_data.php)"; 

    // 📌 Konfigurasi Pin Sensor
    const int TRIG_PIN = 23; 
    const int ECHO_PIN = 22; 
    ```

3.  **Kalibrasi (Sangat Penting):** Sesuaikan nilai jarak minimum dan maksimum Anda untuk konversi Level (%) yang akurat:

    ```cpp
    // Nilai dalam cm
    const float D_MAX = 250.0; // Jarak sensor ke dasar (Level 0%)
    const float D_MIN = 10.0;  // Jarak sensor ke air saat dianggap BANJIR (Level 100%)
    ```

4.  *Upload* kode ke ESP32 Anda.

### Langkah 3: Akses Dashboard

1.  Setelah *upload* berhasil, ESP32 akan mulai mengirim data.
2.  Akses *dashboard* melalui *browser*: `http://alamat.server.anda/index.html` (atau nama file *dashboard* Anda).

---

## 💡 Konversi Tingkat Ketinggian Air

Ketinggian air dalam persentase dihitung dengan mempertimbangkan jarak kalibrasi ($D_{max}$ dan $D_{min}$) dan Jarak Terukur ($D_{ukur}$):

$$\text{Level}(\%) = \left( 1 - \frac{D_{ukur} - D_{min}}{D_{max} - D_{min}} \right) \times 100$$

Ini memastikan bahwa semakin kecil $D_{ukur}$ (sensor semakin dekat ke air), semakin tinggi persentase Level (mendekati 100%).
