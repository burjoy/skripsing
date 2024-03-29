#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "BluetoothSerial.h"
#include "TimeLib.h"
#include <ESP32SPISlave.h>
#include "driver/twai.h"
#include <ESP32Time.h>

// Check if Bluetooth configs are enabled
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Bluetooth Serial object
BluetoothSerial SerialBT;

#define RX_PIN 25 //tar ganti ke mana (jangan 4 dan 5) buat akomodasi rtc ama clk
#define TX_PIN 33 // tar ganti ke mana (jangan 4 dan 5) buat akomodasi rtc ama miso

tmElements_t my_time;  // time elements structure
time_t unix_timestamp; // a timestamp
char data;

String message = "";
String datetime_awal;
String datetime_akhir;
String request;
String waktu_end;
String waktu_start;
String permintaan;
String nama_file;
String hasil_baca_file;
String lokasi_file;
const char* tempat_file;

int first_slice;
int second_slice;
int third_slice;
int fourth_slice;
long int prevMillis = 0;
int nyalain_bluetooth = 0;
int matiin_bluetooth = 0;

char incomingChar;
int stop = 0;
bool deviceConnected = false;
TaskHandle_t spiTaskHandle = NULL;
const char* data_vcu_now_cstr;

ESP32SPISlave slave;
ESP32Time rtc(3600);

twai_message_t vcu_message;
twai_message_t send_to_vcu;

static constexpr uint32_t BUFFER_SIZE {32};
uint8_t spi_slave_tx_buf[BUFFER_SIZE];
uint8_t spi_slave_rx_buf[BUFFER_SIZE];

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);
  File file = fs.open(path, FILE_WRITE);
  
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  
  file.close();
}

void spiTask(void *pvParameters) {
  while (1) {
    processSPI();
    vTaskDelay(pdMS_TO_TICKS(1000)); // Adjust the delay as needed
  }
}

void twai_setup_and_install_for_send(){
    //Initialize configuration structures using macro initializers
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install TWAI driver for bus 1
    // g_config.controller_id = 0;
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        printf("Driver installed\n");
    } else {
        printf("Failed to install driver\n");
        return;
    }

    // Start TWAI driver
    if (twai_start() == ESP_OK) {
        printf("Driver started\n");
    } else {
        printf("Failed to start driver\n");
        return;
    }
}

void readFile(fs::FS &fs, long int start_date, long int end_date, String filter) {
  // Serial.printf("Reading file: %s\n", path);

  if(filter.indexOf("engine") != -1 || filter.indexOf("inverter") != -1 || filter.indexOf("pdu") != -1){
    nama_file = "_inverterPDUState.txt";
  }
  else if(filter.indexOf("dcdc") != -1 || filter.indexOf("cell_voltage") != -1){
    nama_file = "_dcdcBMSState.txt";
  }
  for(long int epoch = start_date;epoch <= end_date;epoch+=86400){
    lokasi_file = String(epoch) + nama_file;
    tempat_file = lokasi_file.c_str();
    File file = fs.open(tempat_file, FILE_READ);
    if (!file) {
      Serial.println("Failed to open file for reading");
      String gagal = "\ngagal gan, file gk ada";
      SerialBT.print(gagal);
      SerialBT.println();
      delay(10);
      // return;
    }

    Serial.print("Read from file: ");
    while (file.available()) {
      // SerialBT.print(file.read());
      // SerialBT.println();
      hasil_baca_file = file.readString();
      if(hasil_baca_file.indexOf(filter) != -1){
        SerialBT.print(hasil_baca_file);
      }
      delay(10);
    }
    file.close();
  }
}

// void readBluetooth(void *arg) {
//   while (1) {
//     if (SerialBT.available()) {
//       incomingChar = SerialBT.read();
//       if (incomingChar != '\n') {
//         message += String(incomingChar);
//       } else {
//         message = "";
//       }
//     }
//     SerialBT.print(message);
//     if (message == "baca inverter") {
//       readFile(SD, "/inverter_status.txt");
//       printf("\nPanjang message: %d", message.length());
//     }
//     if (message == "baca pdu") {
//       readFile(SD, "/pdu_status.txt");
//       printf("\nPanjang message: %d", message.length());
//     }
//     if (message == "baca dcdc") {
//       readFile(SD, "/dcdc_state.txt");
//       printf("\nPanjang message: %d", message.length());
//     }
//     message = "";
//   }
// }

void read_request() {
    // processSPI();
    if (SerialBT.available()) {
      incomingChar = SerialBT.read();
      // if(isSpace(incomingChar)){
      //   incomingChar = ',';
      // }
      if (incomingChar != '\n') {
        if(isSpace(incomingChar)){
          incomingChar = ',';
        }
        message += String(incomingChar);
      }
       else {
        // Process and send data only if there is a valid input
        if (!message.isEmpty()) {
          first_slice = message.indexOf(',');
          datetime_awal = message.substring(0, first_slice);

          second_slice = message.indexOf(',', first_slice+1);
          datetime_akhir = message.substring(first_slice + 1, second_slice);

          third_slice = message.indexOf(',', second_slice+1);
          request = message.substring(second_slice+1, third_slice);

          // printf("\ndatetime akhir: %s", datetime_akhir);
          // printf("\ndatetime awal: %s", datetime_awal);
          // printf("\nrequest: %s", request);
          // printf("\nrequest: %s", request);

          int year_start = datetime_awal.substring(0, 4).toInt();
          int month_start = datetime_awal.substring(5, 7).toInt();
          int day_start = datetime_awal.substring(8, 10).toInt();
          int hour_start = datetime_awal.substring(11, 13).toInt();
          int minute_start = datetime_awal.substring(14, 16).toInt();

          int year_end = datetime_akhir.substring(0, 4).toInt();
          int month_end = datetime_akhir.substring(5, 7).toInt();
          int day_end = datetime_akhir.substring(8, 10).toInt();
          int hour_end = datetime_akhir.substring(11, 13).toInt();
          int minute_end = datetime_akhir.substring(14, 16).toInt();

          long int waktu_mulai = change_to_unix(year_start, month_start, day_start);
          long int waktu_akhir = change_to_unix(year_end, month_end, day_end);

          //komen ama unkomen 2 process request dibawah ini buat demo & testing

          process_request(waktu_mulai, waktu_akhir, request);

          // if(data == 1){
          //   process_request(waktu_mulai, waktu_akhir, request);
          // }
          // else{
          //   SerialBT.print("\nAkses tidak tersedia");
          // }
          // SerialBT.print(waktu_mulai);
          // SerialBT.print(datetime_akhir);
          // SerialBT.print(request);
          // SerialBT.print(message);

          // datetime_awal = "";
          // datetime_akhir = "";
          // request = "";
          printf("\nPesen: %s", message);
          // SerialBT.end();
          // printf("\ncoba liat mati apa kagak");
          message = "";
        }
      }
    }
}

long int change_to_unix(int tahun, int bulan, int hari){
  my_time.Second = 0;
  my_time.Hour = 24;
  my_time.Minute = 0;
  my_time.Day = hari - 1; //gak tau napa harus -1 hari, harusnya bulan
  my_time.Month = bulan;      //jangan di -1, kgk tau napa, harusnya di -1
  my_time.Year = tahun - 1970; // years since 1970, so deduct 1970

  unix_timestamp =  makeTime(my_time);
  // Serial.println(unix_timestamp);
  long int waktu = int(unix_timestamp);
  return waktu;
}

void process_request(long int waktu_mulai, long int waktu_akhir, String request){
  printf("\n unix awal: %d, \t unix akhir: %d, \t request: %s", waktu_mulai, waktu_akhir, request);
  waktu_start = "Waktu Mulai: " + String(waktu_mulai);
  waktu_end = "\nWaktu Akhir: " + String(waktu_akhir);
  permintaan = "\nRequest: " + request;
  int kiriman = request.toInt();
  // kiriman = request.c_str();
  // String test = "Test ajg\n";
  // char data;
  // while (slave.available()) {
  //   // show received data
  //   Serial.print("Command Received: ");
  //   Serial.println(spi_slave_rx_buf[0]);
  //   data = spi_slave_rx_buf[0];
  //   slave.pop();
  //   // }
  //   if(data == 1 )
  //   {
  //       Serial.println("Setting LED active HIGH ");
  //   }
  //   else if(data == 0 )
  //   {
  //       Serial.println("Setting LED active LOW ");
  //   }
  // }
  Serial.println("");
  if(waktu_mulai > waktu_akhir){
    SerialBT.print("\nInvalid Request");
    return;
  }
  SerialBT.print(waktu_start);
  SerialBT.print(waktu_end);
  SerialBT.print(permintaan);

  switch(kiriman){
    case 1:
      SerialBT.print("\nTegangan Inverter");
      readFile(SD, waktu_mulai, waktu_akhir, "inverter_voltage");
      break;
    case 2:
      SerialBT.print("\nSpeed");
      readFile(SD, waktu_mulai, waktu_akhir, "engine_speed");
      break;
    case 3:
      SerialBT.print("\nRPM");
      readFile(SD, waktu_mulai, waktu_akhir, "inverter_rpm");
      break;
    case 4:
      SerialBT.print("\nTemperature");
      readFile(SD, waktu_mulai, waktu_akhir, "engine_temp");
      break;
    case 5:
      SerialBT.print("\nDCDC Current");
      readFile(SD, waktu_mulai, waktu_akhir, "dcdc_current");
      break;
    case 6:
      SerialBT.print("\nCell battery voltage");
      readFile(SD, waktu_mulai, waktu_akhir, "cell_voltage");
      break;
    // default:
    //   SerialBT.print("\nInvalid Code Request");
    //   break;
  }

  waktu_start = "";
  waktu_end = "";
  permintaan = "";
}

void getFromVCU(void *arg){
  while(1){
    long int currentMillis = millis();
    if(twai_receive(&vcu_message, pdMS_TO_TICKS(1000)) == ESP_OK){
      if(currentMillis - prevMillis >= 8000){
        if(data == 0){
          printf("\nDapetin data dari vcu lu anjeng");
        }
        if(data == 1){
          printf("\nJangan ambil data dulu");
        }
        prevMillis = currentMillis;
      }
    }
    else{
      //ini buat contoh aja, tar ilangin lagi
      if(currentMillis - prevMillis >= 8000){
        if(data == 0){
          printf("\nData gk dapet tai");
        }
        if(data == 1){
          printf("\nGw bilang kgk dapet data");
        }
        prevMillis = currentMillis;
      }      
    }
    vTaskDelay(100);
  }
}

// void write_to_sd(){

// }

void processSPI(){
  slave.wait(spi_slave_rx_buf, spi_slave_tx_buf, BUFFER_SIZE);
    // show received data
  Serial.print("Command Received: ");
  Serial.println(spi_slave_rx_buf[0]);
  data = spi_slave_rx_buf[0];
  slave.pop();
    // }
  if(data == 1 )
  {
    Serial.println("Setting LED active HIGH ");
    if(nyalain_bluetooth != 1){
      SerialBT.begin("ESP32-Slave");
      nyalain_bluetooth = 1;
      matiin_bluetooth = 0;
    }
    // digitalWrite(15, HIGH);
  }
  else if(data == 0 )
  {
    Serial.println("Setting LED active LOW ");
    if(matiin_bluetooth != 1){
      SerialBT.end();
      nyalain_bluetooth = 0;
      matiin_bluetooth = 1;
    }
    //inget, serialbt.end buat end bluetooth
  }
  else if(data == 5){
    write_data();
  }
}

void write_data(){
  Serial.println("Checked program");
}

//inget, epoch +1 hari itu +86400

void decodeData(unsigned long combined_data) {
  int year = (combined_data >> 48) & 0xFFFF; // Extract year (bits 48-63)
  int month = (combined_data >> 40) & 0xFF;   // Extract month (bits 40-47)
  int day = (combined_data >> 32) & 0xFF;     // Extract day (bits 32-39)
  int hour = (combined_data >> 24) & 0xFF;    // Extract hour (bits 24-31)
  int minute = (combined_data >> 16) & 0xFF;  // Extract minute (bits 16-23)
  int second = (combined_data >> 8) & 0xFF;   // Extract second (bits 8-15)

  // Do something with the decoded data, such as printing it
  Serial.print("Received Date and Time: ");
  Serial.print(year);
  Serial.print("-");
  Serial.print(month);
  Serial.print("-");
  Serial.print(day);
  Serial.print(" ");
  Serial.print(hour);
  Serial.print(":");
  Serial.print(minute);
  Serial.print(":");
  Serial.println(second);
}

void setup() {
  byte datetimeBytes[7];
  slave.setDataMode(SPI_MODE0);
  slave.begin();

  memset(spi_slave_tx_buf, 0, BUFFER_SIZE);
  memset(spi_slave_rx_buf, 0, BUFFER_SIZE);

  slave.wait(spi_slave_rx_buf, spi_slave_tx_buf, BUFFER_SIZE);

  // Decode received data
  unsigned long combined_data;
  for (int i = 0; i < BUFFER_SIZE; i++) {
    // combined_data |= (unsigned long)spi_slave_rx_buf[i] << (8 * (BUFFER_SIZE - 1 - i));
    datetimeBytes[i] = spi_slave_rx_buf[i];
  }
  // Serial.println(combined_data);
  // decodeData(combined_data);
  Serial.println(datetimeBytes[2]);

  slave.pop(); // Clear the buffer

  int year = (datetimeBytes[0] << 8) | datetimeBytes[1];
  byte month = datetimeBytes[2];
  byte day = datetimeBytes[3];
  byte hour = datetimeBytes[4];
  byte minute = datetimeBytes[5];
  byte second = datetimeBytes[6];

  Serial.println(year);
  Serial.println(hour);

  long int unix_now = change_to_unix(year, int(month), int(day));
  String data_vcu_now = "/" + String(unix_now) + "_vcuNow.txt";
  data_vcu_now_cstr = data_vcu_now.c_str();

  pinMode(32, OUTPUT);
  pinMode(27, OUTPUT);
  pinMode(26, OUTPUT);
  pinMode(4, OUTPUT);

  digitalWrite(32, HIGH);
  digitalWrite(26, HIGH);
  digitalWrite(27, HIGH);
  digitalWrite(4, HIGH);
  
  Serial.begin(115200);

  if (!SD.begin(5)) {
    Serial.println("Card Mount Failed");
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  File file = SD.open(data_vcu_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File vcu belum ada");
    writeFile(SD, data_vcu_now_cstr, "\n");
  }
  else{
    Serial.println("Data vcu filenya sudah ada");
  }

  digitalWrite(32, LOW);
  digitalWrite(26, LOW);
  digitalWrite(27, LOW);
  digitalWrite(4, LOW);

  // delay(200);  

  twai_setup_and_install_for_send();
  // SerialBT.begin("ESP32-Slave");

  xTaskCreatePinnedToCore(spiTask, "spiTask", 3000, NULL, 2, &spiTaskHandle, 1);
  xTaskCreatePinnedToCore(getFromVCU, "get_from_vcu", 3000, NULL, 3, NULL, 1);

  Serial.println("The device started, now you can pair it with Bluetooth!");

  // xTaskCreatePinnedToCore(read_request, "read_request", 3000, NULL, 2, NULL, 0);
  // xTaskCreatePinnedToCore(processSPI, "", 3000, NULL, 2, NULL, 1);
  // vTaskDelete(NULL);
}

void loop() {
  // Your main code here
  // processSPI();
  read_request();
  vTaskDelay(pdMS_TO_TICKS(10)); 
}
