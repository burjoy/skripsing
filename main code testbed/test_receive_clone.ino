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
twai_message_t message_dcdc;

const char* dcdc_state_now_cstr;
const char* pdu_state_now_cstr;
const char* inverter_state_now_cstr;
const char* engine_rpm_now_cstr;
const char* engine_speed_now_cstr;
const char* engine_temp_now_cstr;
const char* inverter_voltage_now_cstr;
const char* inverter_current_now_cstr;
const char* dcdc_current_now_cstr;
const char* dcdc_hv_input_now_cstr;
const char* dcdc_lv_output_now_cstr;

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

String get_epoch_now(){
    DateTime now = rtc.now();
    int hari = now.day();
    int bulan = now.month();
    int tahun = now.year();
    DateTime startOfDay = DateTime(tahun, bulan, hari, 0, 0, 0);
    String epoch = String(startOfDay.unixtime());
    return epoch;
    printf("\nTime in epoch at start of day: %s", epoch);    
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
    if(currentMillis - prevMillis >= 5000){
      if(twai_receive(&message_inverter_speed_rpm_temp, pdMS_TO_TICKS(1000)) == ESP_OK){
        // if(message.identifier == 0x0CF00402){
        //     printf("\nID Inverter: %x", message.identifier);        
        //     File file = SD.open("/inverter_status.txt", FILE_APPEND);
        //     if (!file) {
        //       Serial.println("File not exist yet");
        //     }
        //     else{
        //       Serial.println("File inverter status already exist");
        //     }
        //     uint8_t status = message.data[0];
        //     if(status == 1){
        //       file.println("Over Current");
        //     }
        //     else if(status == 2){
        //       file.println("Over Voltage");
        //     }
        //     printf("\n");
        //   }
            if(message_inverter_speed_rpm_temp.identifier == 0x0CF00401){
              printf("\nID Inverter: %x", message_inverter_speed_rpm_temp.identifier);        
              File file = SD.open(engine_speed_now_cstr, FILE_APPEND);
              if (!file) {
                Serial.println("File not exist yet");
              }
              else{
                Serial.println("File engine speed already exist");
              }
              uint8_t speed_msb = message_inverter_speed_rpm_temp.data[2];
              uint8_t speed_lsb = message_inverter_speed_rpm_temp.data[1];
              uint16_t engine_speed = (speed_msb << 8) | speed_lsb;
              int real_speed = engine_speed * 0.125;
              String speedy = waktu + "\t" + String(real_speed);
              file.println(speedy);
              file.close();

              file = SD.open(engine_rpm_now_cstr, FILE_APPEND);
              if (!file) {
                Serial.println("File not exist yet");
              }
              else{
                Serial.println("File engine rpm already exist");
              }
              uint8_t rpm_msb = message_inverter_speed_rpm_temp.data[4];
              uint8_t rpm_lsb = message_inverter_speed_rpm_temp.data[3];
              uint16_t engine_rpm = (rpm_msb << 8) | rpm_lsb;
              int real_rpm = engine_rpm * 0.125;
              String rpms = waktu + "\t" + String(real_rpm);
              file.println(rpms);
              file.close();

              file = SD.open(engine_temp_now_cstr, FILE_APPEND);
              if (!file) {
                Serial.println("File not exist yet");
              }
              else{
                Serial.println("File engine temp already exist");
              }
              uint8_t temp_msb = message_inverter_speed_rpm_temp.data[6];
              uint8_t temp_lsb = message_inverter_speed_rpm_temp.data[5];
              uint16_t engine_temp = (temp_msb << 8) | temp_lsb;
              // int real_rpm = engine_rpm * 0.125;
              String temps = waktu + "\t" + String(engine_temp);
              file.println(temps);
              file.close();
          }
        }
      if(twai_receive(&message_inverter_voltage_current, pdMS_TO_TICKS(1000)) == ESP_OK){
        // if(message.identifier == 0x0CF00402){
        //     printf("\nID Inverter: %x", message.identifier);        
        //     File file = SD.open("/inverter_status.txt", FILE_APPEND);
        //     if (!file) {
        //       Serial.println("File not exist yet");
        //     }
        //     else{
        //       Serial.println("File inverter status already exist");
        //     }
        //     uint8_t status = message.data[0];
        //     if(status == 1){
        //       file.println("Over Current");
        //     }
        //     else if(status == 2){
        //       file.println("Over Voltage");
        //     }
        //     printf("\n");
        //   }
            if(message_inverter_voltage_current.identifier == 0x0CF00402){
              printf("\nID Inverter: %x", message_inverter_voltage_current.identifier);        
              File file = SD.open(inverter_voltage_now_cstr, FILE_APPEND);
              if (!file) {
                Serial.println("File not exist yet");
              }
              else{
                Serial.println("File inverter voltage already exist");
              }
              uint8_t voltage_msb = message_inverter_voltage_current.data[1];
              uint8_t voltage_lsb = message_inverter_voltage_current.data[0];
              uint16_t inverter_voltage = (voltage_msb << 8) | voltage_lsb;
              // int real_speed = engine_speed * 0.125;
              String tegangan = waktu + "\t" + inverter_voltage;
              file.println(tegangan);
              file.close();

              file = SD.open(inverter_current_now_cstr, FILE_APPEND);
              if (!file) {
                Serial.println("File not exist yet");
              }
              else{
                Serial.println("File inverter current already exist");
              }
              uint8_t current_msb = message_inverter_voltage_current.data[3];
              uint8_t current_lsb = message_inverter_voltage_current.data[2];
              uint16_t inverter_current = (current_msb << 8) | current_lsb;
              // int real_rpm = engine_rpm * 0.125;
              String arus = waktu + "\t" + inverter_current;
              file.println(arus);
              file.close();
          }
        }
      prevMillis = currentMillis;
      printf("\nWaktu kemakan buat inverter: %d", currentMillis);
    }
  }
}

void read_pdu_state1(void *arg){
  while(1){
  long int currentMillis = millis();
  if(currentMillis - prevMillis >= 2000){
  if(twai_receive(&message2, pdMS_TO_TICKS(1000)) == ESP_OK){
    switch(message2.identifier){
      case 0x10F51402:
        printf("\nID PDU1: %x", message.identifier);
        File file = SD.open(pdu_state_now_cstr, FILE_APPEND);
        if (!file) {
          Serial.println("File does not exist yet");
        }
        else{
          Serial.println("File pdu status already exist");
        }
        // uint8_t status_bms_msb = message->data[1];
        // uint8_t status_bms_lsb = message->data[0];
        for(int i = 0; i < message2.data_length_code; i++){
          printf("%x\t", message2.data[i]);
          //  // Append data to the file
           if (file.print(String(message2.data[i]))) {
              Serial.println("Message pdu1 appended");
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
    printf("\nWaktu kemakan buat pdu1: %d", currentMillis);
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

void read_dcdc_state(void *arg){
  while(1){
  long int currentMillis = millis();
  if(currentMillis - prevMillis >= 2000){
    if(twai_receive(&message_dcdc, pdMS_TO_TICKS(1000)) == ESP_OK){
      switch(message_dcdc.identifier){
        case 0x18F11401:
          printf("\nID DCDC: %x", message_dcdc.identifier);
          File file = SD.open(dcdc_current_now_cstr, FILE_APPEND);
          if(!file){
            Serial.println("File dcdc current now does not exist yet");
          }
          else{
            Serial.println("File dcdc current already exist");          
          }
          uint8_t current_msb = message_dcdc.data[3];
          uint8_t current_lsb = message_dcdc.data[2];
          uint16_t current = (current_msb << 8) | current_lsb;
          String arus = waktu + "\t" + String(current);
          file.println(arus);
          file.close();

          file = SD.open(dcdc_hv_input_now_cstr, FILE_APPEND);
          uint8_t hv_msb = message_dcdc.data[5];
          uint8_t hv_lsb = message_dcdc.data[4];
          uint16_t hv = (hv_msb << 8) | hv_lsb;
          String high_voltage = waktu + "\t" + String(hv);
          file.println(high_voltage);
          file.close();

          file = SD.open(dcdc_lv_output_now_cstr, FILE_APPEND);
          uint8_t lv_msb = message_dcdc.data[7];
          uint8_t lv_lsb = message_dcdc.data[6];
          uint16_t lv = (lv_msb << 8) | lv_lsb;
          String low_voltage = waktu + "\t" + String(lv);
          file.println(low_voltage);
          file.close();                              
          }
        }
      prevMillis = currentMillis;
      printf("\nWaktu abis buat dcdc: %d", currentMillis);
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

  epoch_now = get_epoch_now();

  String dcdc_state_now = "/" + epoch_now + "_dcdcState.txt";
  String pdu_state_now = "/" + epoch_now + "_pduState.txt";
  String inverter_state_now = "/" + epoch_now + "_inverterState.txt";
  String engine_speed_now = "/" + epoch_now + "_engineSpeed.txt";
  String engine_rpm_now = "/" + epoch_now + "_engineRPM.txt";
  String inverter_current_now = "/" + epoch_now + "_inverterCurrent.txt";
  String inverter_voltage_now = "/" + epoch_now + "_inverterVoltage.txt";
  String engine_temp_now = "/" + epoch_now + "_engineTemp.txt";
  String dcdc_current_now = "/" + epoch_now + "_dcdcCurrent.txt";
  String dcdc_hv_input_now = "/" + epoch_now + "_dcdcHVinput.txt";
  String dcdc_lv_output_now = "/" + epoch_now + "_dcdcLVoutput.txt";

  dcdc_state_now_cstr = dcdc_state_now.c_str();
  pdu_state_now_cstr = pdu_state_now.c_str();
  inverter_state_now_cstr = inverter_state_now.c_str();
  inverter_current_now_cstr = inverter_current_now.c_str();
  inverter_voltage_now_cstr = inverter_voltage_now.c_str();
  engine_speed_now_cstr = engine_speed_now.c_str();
  engine_rpm_now_cstr = engine_rpm_now.c_str();
  engine_temp_now_cstr = engine_temp_now.c_str();
  dcdc_current_now_cstr = dcdc_current_now.c_str();
  dcdc_hv_input_now = dcdc_hv_input_now.c_str();
  dcdc_lv_output_now = dcdc_lv_output_now.c_str();

  File file  = SD.open(dcdc_state_now_cstr, FILE_APPEND);
  if(!file) {
    Serial.println("File dcdc state for today doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, dcdc_state_now_cstr, "test\r\n");
  }
  else {
    Serial.println("File dcdc state already exists");  
  }
  
  file.close();

  file = SD.open(pdu_state_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File pdu state for today does not exist yet");
    writeFile(SD, pdu_state_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File pdu state already exist");
  }
  file.close();

  // file = SD.open("/bms_status.txt", FILE_APPEND);
  // if(!file){
  //   Serial.println("File does not exist yet");
  //   writeFile(SD, "/bms_status.txt", "test\r\n");
  // }
  // else{
  //   Serial.println("File bms state already exist");
  // }
  // file.close();

  file = SD.open(inverter_current_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File inverter current for today does not exist yet");
    writeFile(SD, inverter_current_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File inverter current already exist");
  }
  file.close();

  file = SD.open(inverter_state_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File inverter state for today does not exist yet");
    writeFile(SD, inverter_state_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File inverter state already exist");
  }
  file.close();

  file = SD.open(inverter_voltage_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File inverter voltage for today does not exist yet");
    writeFile(SD, inverter_voltage_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File inverter voltage already exist");
  }
  file.close();

  file = SD.open(engine_speed_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File engine_speed for today does not exist yet");
    writeFile(SD, engine_speed_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File engine speed already exist");
  }
  file.close();

  file = SD.open(engine_rpm_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File engine rpm for today does not exist yet");
    writeFile(SD, engine_rpm_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File engine rpm already exist");
  }
  file.close();

  file = SD.open(engine_temp_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File engine temp for today does not exist yet");
    writeFile(SD, engine_temp_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File engine temp already exist");
  }
  file.close();

  file = SD.open(dcdc_current_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File dcdc current for today does not exist yet");
    writeFile(SD, dcdc_current_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File dcdc current already exist");
  }
  file.close();

  file = SD.open(dcdc_hv_input_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File dcdc hv input for today does not exist yet");
    writeFile(SD, dcdc_hv_input_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File dcdc hv input already exist");
  }
  file.close();

  file = SD.open(dcdc_lv_output_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File dcdc lv output for today does not exist yet");
    writeFile(SD, dcdc_lv_output_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File dcdc lv output already exist");
  }
  file.close();

  // xTaskCreate(receive_message, "receiveMessage", 3000, NULL, 2, &receiveMessageHandle);
  xTaskCreate(read_inverter_status, "inverterStatus", 3000, NULL, 3, &inverterStatusHandle);
  xTaskCreate(read_pdu_state1, "pduState1", 3000, NULL, 2, &pdu123Handle);
  xTaskCreatePinnedToCore(read_pdu_state2, "pduState2", 3000, NULL, 2, &pdu123Handle, 1);
  xTaskCreatePinnedToCore(read_pdu_state3, "pduState3", 3000, NULL, 1, &pdu123Handle, 1);
  xTaskCreate(read_dcdc_state, "dcdcState", 3000, NULL, 1, &dcdcStateHandle);
  vTaskDelete(NULL);

  //inget, priority makin tinggi, resource pool ke tugas itu makin banyak
}

void loop()
{

}
