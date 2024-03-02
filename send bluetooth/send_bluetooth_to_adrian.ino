#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "BluetoothSerial.h"
#include "TimeLib.h"
#include <ESP32SPISlave.h>

// Check if Bluetooth configs are enabled
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Bluetooth Serial object
BluetoothSerial SerialBT;

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

int first_slice;
int second_slice;
int third_slice;
int fourth_slice;

char incomingChar;
int stop = 0;
bool deviceConnected = false;
TaskHandle_t spiTaskHandle = NULL;

ESP32SPISlave slave;

static constexpr uint32_t BUFFER_SIZE {32};
uint8_t spi_slave_tx_buf[BUFFER_SIZE];
uint8_t spi_slave_rx_buf[BUFFER_SIZE];

void spiTask(void *pvParameters) {
  while (1) {
    processSPI();
    vTaskDelay(pdMS_TO_TICKS(10)); // Adjust the delay as needed
  }
}

void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    String gagal = "gagal gan";
    SerialBT.print(gagal);
    SerialBT.println();
    delay(10);
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    SerialBT.print(file.read());
    SerialBT.println();
  }
  file.close();
}

void readBluetooth(void *arg) {
  while (1) {
    if (SerialBT.available()) {
      incomingChar = SerialBT.read();
      if (incomingChar != '\n') {
        message += String(incomingChar);
      } else {
        message = "";
      }
    }
    SerialBT.print(message);
    if (message == "baca inverter") {
      readFile(SD, "/inverter_status.txt");
      printf("\nPanjang message: %d", message.length());
    }
    if (message == "baca pdu") {
      readFile(SD, "/pdu_status.txt");
      printf("\nPanjang message: %d", message.length());
    }
    if (message == "baca dcdc") {
      readFile(SD, "/dcdc_state.txt");
      printf("\nPanjang message: %d", message.length());
    }
    message = "";
  }
}

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

          process_request(waktu_mulai, waktu_akhir, request);

          // SerialBT.print(waktu_mulai);
          // SerialBT.print(datetime_akhir);
          // SerialBT.print(request);
          // SerialBT.print(message);

          // datetime_awal = "";
          // datetime_akhir = "";
          // request = "";
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
      SerialBT.print("\nDashboard");
      break;
    case 2:
      SerialBT.print("\nSpeeds");
      break;
    case 3:
      SerialBT.print("\nRPM");
      break;
    case 4:
      SerialBT.print("\nTemperature");
      break;
    case 5:
      SerialBT.print("\nBattery temperature");
      break;
    case 6:
      SerialBT.print("\nCell battery");
      break;
    // default:
    //   SerialBT.print("\nInvalid Code Request");
    //   break;
  }

  waktu_start = "";
  waktu_end = "";
  permintaan = "";
}

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
    // digitalWrite(15, HIGH);
  }
  else if(data == 0 )
  {
    Serial.println("Setting LED active LOW ");
  }
}

//inget, epoch +1 hari itu +86400

void setup() {
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

  slave.setDataMode(SPI_MODE0);
  slave.begin();

  memset(spi_slave_tx_buf, 0, BUFFER_SIZE);
  memset(spi_slave_rx_buf, 0, BUFFER_SIZE);

  xTaskCreatePinnedToCore(spiTask, "spiTask", 3000, NULL, 2, &spiTaskHandle, 1);

  SerialBT.begin("ESP32-Slave");

  Serial.println("The device started, now you can pair it with Bluetooth!");

  // xTaskCreatePinnedToCore(read_request, "read_request", 3000, NULL, 2, NULL, 0);
  // xTaskCreatePinnedToCore(processSPI, "processSPI", 3000, NULL, 2, NULL, 1);
  // vTaskDelete(NULL);
}

void loop() {
  // Your main code here
  // processSPI();
  read_request();
  vTaskDelay(pdMS_TO_TICKS(10)); 
}
