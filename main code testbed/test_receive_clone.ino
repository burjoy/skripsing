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

void read_inverter_status(void *arg){
  while(1){
    long int currentMillis = millis();
    if(currentMillis - prevMillis >= 7000){
      if(twai_receive(&message_inverter_speed_rpm_temp, pdMS_TO_TICKS(1000)) == ESP_OK){
            if(message_inverter_speed_rpm_temp.identifier == 0x0CF00401){
              printf("\nID Inverter: %x", message_inverter_speed_rpm_temp.identifier);        
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
              File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
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
              File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
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
        if(twai_receive(&message_pdu_dcdc_input_output_voltage, pdMS_TO_TICKS(1000)) == ESP_OK){
            if(message_pdu_dcdc_input_output_voltage.identifier == 0x18FE8D02){
              printf("\nID Inverter: %x", message_pdu_dcdc_input_output_voltage.identifier);        
              uint8_t input_voltage_msb = message_pdu_dcdc_input_output_voltage.data[2];
              uint8_t input_voltage_lsb = message_pdu_dcdc_input_output_voltage.data[1];
              uint16_t pdu_input_voltage = (input_voltage_msb << 8) | input_voltage_lsb;
              // int real_speed = engine_speed * 0.125;
              String input_pdu_tegangan = waktu + "\t" + pdu_input_voltage + "\t\t" + "pdu_input_voltage";

              uint8_t output_voltage_msb = message_pdu_dcdc_input_output_voltage.data[4];
              uint8_t output_voltage_lsb = message_pdu_dcdc_input_output_voltage.data[3];
              uint16_t pdu_output_voltage = (output_voltage_msb << 8) | output_voltage_lsb;
              // int real_speed = engine_speed * 0.125;
              String output_pdu_tegangan = waktu + "\t" + pdu_output_voltage + "\t\t" + "pdu_output_voltage";

              uint8_t pdu_dcdc_current_msb = message_pdu_dcdc_input_output_voltage.data[6];
              uint8_t pdu_dcdc_current_lsb = message_pdu_dcdc_input_output_voltage.data[5];
              uint16_t pdu_dcdc_current = (pdu_dcdc_current_msb << 8) | pdu_dcdc_current_lsb;
              // int real_rpm = engine_rpm * 0.125;
              String pdu_dcdc_arus = waktu + "\t" + pdu_dcdc_current + "\t\t" + "pdu_dcdc_current";
              File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
              if (!file) {
                Serial.println("File not exist yet");
              }
              else{
                Serial.println("File inverter voltage already exist");
              }
              file.println(input_pdu_tegangan);
              file.println(output_pdu_tegangan);
              file.println(pdu_dcdc_arus);
              file.close();
          }
        }
        if(twai_receive(&message_pdu_hv_precharge_ecu, pdMS_TO_TICKS(1000)) == ESP_OK){
            if(message_pdu_hv_precharge_ecu.identifier == 0x18FE8D01){
              printf("\nID Inverter: %x", message_pdu_hv_precharge_ecu.identifier);        
              uint8_t input_voltage_msb = message_pdu_hv_precharge_ecu.data[2];
              uint8_t input_voltage_lsb = message_pdu_hv_precharge_ecu.data[1];
              uint16_t pdu_input_voltage = (input_voltage_msb << 8) | input_voltage_lsb;
              // int real_speed = engine_speed * 0.125;
              String input_pdu_tegangan = waktu + "\t" + pdu_input_voltage + "\t\t" + "pdu_input_voltage";

              uint8_t output_voltage_msb = message_pdu_hv_precharge_ecu.data[4];
              uint8_t output_voltage_lsb = message_pdu_hv_precharge_ecu.data[3];
              uint16_t pdu_output_voltage = (output_voltage_msb << 8) | output_voltage_lsb;
              // int real_speed = engine_speed * 0.125;
              String output_pdu_tegangan = waktu + "\t" + pdu_output_voltage + "\t\t" + "pdu_output_voltage";

              uint8_t pdu_dcdc_current_msb = message_pdu_hv_precharge_ecu.data[6];
              uint8_t pdu_dcdc_current_lsb = message_pdu_hv_precharge_ecu.data[5];
              uint16_t pdu_dcdc_current = (pdu_dcdc_current_msb << 8) | pdu_dcdc_current_lsb;
              // int real_rpm = engine_rpm * 0.125;
              String pdu_dcdc_arus = waktu + "\t" + pdu_dcdc_current + "\t\t" + "pdu_dcdc_current";
              File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
              if (!file) {
                Serial.println("File not exist yet");
              }
              else{
                Serial.println("File inverter voltage already exist");
              }
              file.println(input_pdu_tegangan);
              file.println(output_pdu_tegangan);
              file.println(pdu_dcdc_arus);
              file.close();
          }
        }
        if(twai_receive(&message_pdu_lv_voltage, pdMS_TO_TICKS(1000)) == ESP_OK){
            if(message_pdu_lv_voltage.identifier == 0x18FE8D03){
              printf("\nID Inverter: %x", message_pdu_lv_voltage.identifier);        
              uint8_t pdu_voltage_msb = message_pdu_lv_voltage.data[2];
              uint8_t pdu_voltage_lsb = message_pdu_lv_voltage.data[1];
              uint16_t pdu_voltage = (pdu_voltage_msb << 8) | pdu_voltage_lsb;
              // int real_speed = engine_speed * 0.125;
              String pdu_tegangan = waktu + "\t" + pdu_voltage + "\t\t" + "pdu_voltage";

              File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
              if (!file) {
                Serial.println("File not exist yet");
              }
              else{
                Serial.println("File inverter voltage already exist");
              }
              file.println(pdu_tegangan);
              file.close();
          }
        }
      prevMillis = currentMillis;
      printf("\nWaktu kemakan buat inverter: %d", millis());
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

  xTaskCreate(read_inverter_status, "inverterStatus", 3000, NULL, 2, &inverterStatusHandle);
  vTaskDelete(NULL);

  //inget, priority makin tinggi, resource pool ke tugas itu makin banyak
}

void loop()
{

}
