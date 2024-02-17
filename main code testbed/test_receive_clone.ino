#include "driver/twai.h"
#include <SD.h>
#include "SPI.h"
#include "FS.h"
#include "BluetoothSerial.h"
#include "RTClib.h"
#include <SPI.h>

#define RX_PIN 25 //tar ganti ke mana (jangan 4 dan 5) buat akomodasi rtc ama clk
#define TX_PIN 33 // tar ganti ke mana (jangan 4 dan 5) buat akomodasi rtc ama miso

long int prevMillis = 0;
RTC_DS1307 rtc;

TaskHandle_t readStatusHandle = NULL;
TaskHandle_t receiveMessageHandle = NULL;
TaskHandle_t inverterStatusHandle = NULL;
TaskHandle_t pdu123Handle = NULL;
TaskHandle_t dcdcStateHandle = NULL;

twai_message_t message;
twai_message_t message2;
twai_message_t message3;
twai_message_t message4;
twai_message_t message5;

//dibawah ini buat inverter, taro di inverter state
twai_message_t message_inverter_speed_rpm_temp;
twai_message_t message_inverter_voltage_current;
twai_message_t message_pdu_dcdc_input_output_voltage;
twai_message_t message_pdu_hv_precharge_ecu;
twai_message_t message_pdu_lv_voltage;
twai_message_t message_dcdc;

const char* dcdc_state_now_cstr;
const char* inverter_pdu_state_now_cstr;

String waktu;
String epoch_now;

String take_time(){
    DateTime now = rtc.now();
    int hari = now.day();
    int bulan = now.month();
    int tahun = now.year();
    String pembatas = "-";
    String date = hari + pembatas + bulan + pembatas + tahun;

    int jam = now.hour();
    int menit = now.minute();
    int detik = now.second();
    String pembatas2 = ":";
    String waktu = jam + pembatas2 + menit + pembatas2 + detik;
    String pembatas3 = "T";
    String date_time = date + pembatas3 + waktu; 
    return date_time;
}

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

void twai_setup_and_install_for_send(){
    //Initialize configuration structures using macro initializers
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_NORMAL);

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install TWAI driver
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

void new_message(twai_message_t *message, uint32_t id, uint8_t dlc, uint8_t *data)
{
    
    message->flags = TWAI_MSG_FLAG_NONE;
    message->identifier = id;
    message->data_length_code = dlc;
    for (int i = 0; i < dlc; i++) {
        message->data[i] = data[i];
    }
    printf("Message created\nID: %ld DLC: %d Data:\t", message->identifier, message->data_length_code);
    for (int i = 0; i < message->data_length_code; i++) {
        printf("%d\t", message->data[i]);
    }
    printf("\n");
}

void transmit_message(twai_message_t *message)
{
    if (twai_transmit(message, pdMS_TO_TICKS(1000)) == ESP_OK) {
        printf("Message queued for transmission\n");
    } else {
        printf("Failed to send message\n");
    }
}

void read_inverter_status(void *arg){
  while(1){
    long int currentMillis = millis();
    if(currentMillis - prevMillis >= 7000){
      if(twai_receive(&message_inverter_speed_rpm_temp, pdMS_TO_TICKS(1000)) == ESP_OK){
            if(message_inverter_speed_rpm_temp.identifier == 0x0CF00401){
              printf("\nID Inverter: %x", message_inverter_speed_rpm_temp.identifier);        
              File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
              uint8_t speed_msb = message_inverter_speed_rpm_temp.data[2];
              uint8_t speed_lsb = message_inverter_speed_rpm_temp.data[1];
              uint16_t engine_speed = (speed_msb << 8) | speed_lsb;
              int real_speed = engine_speed * 0.125;
              String speedy = waktu + "\t" + String(real_speed) + "\t\t" + "engine_speed";

              uint8_t rpm_msb = message_inverter_speed_rpm_temp.data[4];
              uint8_t rpm_lsb = message_inverter_speed_rpm_temp.data[3];
              uint16_t engine_rpm = (rpm_msb << 8) | rpm_lsb;
              int real_rpm = engine_rpm * 0.125;
              String rpms = waktu + "\t" + String(real_rpm) + "\t\t" + "engine_rpm";

              uint8_t temp_msb = message_inverter_speed_rpm_temp.data[6];
              uint8_t temp_lsb = message_inverter_speed_rpm_temp.data[5];
              uint16_t engine_temp = (temp_msb << 8) | temp_lsb;
              // int real_rpm = engine_rpm * 0.125;
              String temps = waktu + "\t" + String(engine_temp) + "\t\t" + "engine_temp";
              if (!file) {
                Serial.println("File not exist yet");
              }
              else{
                Serial.println("File engine speed already exist");
              }
              file.println(speedy);
              file.println(rpms);
              file.println(temps);
              file.close();
          }
        }
      if(twai_receive(&message_inverter_voltage_current, pdMS_TO_TICKS(1000)) == ESP_OK){
            if(message_inverter_voltage_current.identifier == 0x0CF00402){
              printf("\nID Inverter: %x", message_inverter_voltage_current.identifier);        
              File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
              uint8_t voltage_msb = message_inverter_voltage_current.data[1];
              uint8_t voltage_lsb = message_inverter_voltage_current.data[0];
              uint16_t inverter_voltage = (voltage_msb << 8) | voltage_lsb;
              // int real_speed = engine_speed * 0.125;
              String tegangan = waktu + "\t" + inverter_voltage + "\t\t" + "inverter_voltage";

              uint8_t current_msb = message_inverter_voltage_current.data[3];
              uint8_t current_lsb = message_inverter_voltage_current.data[2];
              uint16_t inverter_current = (current_msb << 8) | current_lsb;
              // int real_rpm = engine_rpm * 0.125;
              String arus = waktu + "\t" + inverter_current + "\t\t" + "inverter_current";
              if (!file) {
                Serial.println("File not exist yet");
              }
              else{
                Serial.println("File inverter voltage already exist");
              }
              file.println(tegangan);
              file.println(arus);
              file.close();
          }
        }
      prevMillis = currentMillis;
      printf("\nWaktu kemakan buat inverter: %d", millis());
    }
  }
}

void read_pdu_state2(void *arg){
  while(1){
  long int currentMillis = millis();
  if(currentMillis - prevMillis >= 2000){
  if(twai_receive(&message3, pdMS_TO_TICKS(1000)) == ESP_OK){
    switch(message3.identifier){
      case 0x18FE8D02:
        printf("\nID PDU2: %x", message3.identifier);
        File file = SD.open("/pdu_status.txt", FILE_APPEND);
        if(!file){
          Serial.println("File does not exist yet");
        }
        else{
          Serial.println("File pdu state already exist");
        }
        uint8_t dcdc_state = message3.data[0];
        uint8_t mock_value[8] = {01, 43, dcdc_state, 00, 00, 00, 00, 00};
        for(int i = 0; i < message3.data_length_code; i++){
          printf("%x\t", mock_value[i]);
          //  // Append data to the file
           if (file.print(String(mock_value[i]))) {
              Serial.println("Message pdu2 appended");
           } else {
              Serial.println("Append failed");
           }
        }
        if (file.print("\nEnter")) {
              Serial.println("Enter appended");
        } else {
              Serial.println("Append failed");
        }
        printf("\n");
      }
    }
    prevMillis = currentMillis;
    printf("\nWaktu kemakan buat pdu2: %d", currentMillis);
  }
  }
}

void read_pdu_state3(void *arg){
  while(1){
  long int currentMillis = millis();
  if(currentMillis - prevMillis >= 2000){
  if(twai_receive(&message4, pdMS_TO_TICKS(1000)) == ESP_OK){
    switch(message4.identifier){
      case 0x18FE8D03:
        printf("\nID PDU3: %x", message4.identifier);
        File file = SD.open("/pdu_state.txt", FILE_APPEND);
        if(!file){
          Serial.println("File does not exist");
        }
        else{
          Serial.println("File pdu state already exist");
        }
        uint8_t lv_state = message4.data[0];
        uint8_t mock_value[8] = {01, 43, 00, lv_state, 00, 00, 00, 00};
        for(int i = 0; i < message4.data_length_code; i++){
          printf("%x\t", mock_value[i]);
          //  // Append data to the file
           if (file.print(String(mock_value[i]))) {
              Serial.println("Message pdu3 appended");
           } else {
              Serial.println("Append failed");
           }
        }
        if (file.print("\nEnter")) {
              Serial.println("Enter appended");
        } else {
              Serial.println("Append failed");
        }
        printf("\n"); 
      }
    }
    prevMillis = currentMillis;
    printf("\nWaktu kemakan buat pdu3: %d", currentMillis);
  }
  }
}

void receive_message(void *arg)
{
  while(1){
    if(millis() - prevMillis >= 1000){
    prevMillis = millis();
    if (twai_receive(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
      Serial.println("Sukses");
       File file = SD.open("/test_log.txt", FILE_APPEND);
       if (!file) {
        Serial.println("Failed to open file for appending");
      }
        printf("\nID Sekarang: %x: ", message.identifier);        
        for(int i = 0; i < message.data_length_code; i++){
          printf("%x\t", message.data[i]);
          //  // Append data to the file
           if (file.print(String(message.data[i]))) {
              Serial.println("Message appended");
           } else {
              Serial.println("Append failed");
           }
        }
        if (file.print("\n")) {
              Serial.println("Enter appended");
        } else {
              Serial.println("Append failed");
        }
        printf("\n");

        switch(message.identifier){
          case 0x0CF00401:
            File file = SD.open("/inverter.txt", FILE_APPEND);
            if (!file) {
              Serial.println("Failed to open file for appending");
            }
            for(int i = 0; i < message.data_length_code; i++){
            printf("%x\t", message.data[i]);
            //  // Append data to the file
            if (file.print(String(message.data[i]))) {
              Serial.println("Message appended for inverter");
            } else {
              Serial.println("Append failed for inverter");
            }
            }
            if (file.print("\n")) {
              Serial.println("Enter appended");
            } else {
              Serial.println("Append failed");
            }
            break;               
        }

    } else {
        Serial.println("Failed to receive message\n");
        Serial.println("gagal nerima pesen\n");
    }
    Serial.println(millis());
  }
  }
}

void setup(){
  // take_time();
  rtc.begin();
  DateTime now = rtc.now();
  int hari = now.day();
  int bulan = now.month();
  int tahun = now.year();
  DateTime startOfDay = DateTime(tahun, bulan, hari, 0, 0, 0);
  int epoch_unix = startOfDay.unixtime();
  epoch_now = String(epoch_unix);
  Serial.begin(115200);
  twai_setup_and_install_for_send();
  if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
    // return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    // return;
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  // epoch_now = "2455";

  String dcdc_state_now = "/" + epoch_now + "_dcdcState.txt";
  String pdu_state_now = "/" + epoch_now + "_pduState.txt";
  String inverter_pdu_state_now = "/" + epoch_now + "_inverterPDUState.txt";

  dcdc_state_now_cstr = dcdc_state_now.c_str();
  inverter_pdu_state_now_cstr = inverter_pdu_state_now.c_str();

  printf("\n");
  printf(dcdc_state_now_cstr);

  File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File inverter and pdu state for today does not exist yet");
    writeFile(SD, inverter_pdu_state_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File inverter and pdu state already exist");
  }
  file.close();

  // xTaskCreate(receive_message, "receiveMessage", 3000, NULL, 2, &receiveMessageHandle);
  xTaskCreate(read_inverter_status, "inverterStatus", 3000, NULL, 2, &inverterStatusHandle);
  // xTaskCreate(read_pdu_state1, "pduState1", 3000, NULL, 2, &pdu123Handle);
  // xTaskCreatePinnedToCore(read_pdu_state2, "pduState2", 3000, NULL, 2, &pdu123Handle, 1);
  // xTaskCreatePinnedToCore(read_pdu_state3, "pduState3", 3000, NULL, 1, &pdu123Handle, 1);
  vTaskDelete(NULL);

  //inget, priority makin tinggi, resource pool ke tugas itu makin banyak
}

void loop()
{

}
