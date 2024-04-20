#include <driver/twai.h>
#include <SD.h>
#include "SPI.h"
#include "FS.h"
#include "BluetoothSerial.h"
#include "RTClib.h"
#include <SPI.h>
#include "driver/gpio.h"
#include <string.h>

#define RX_PIN 25 //tar ganti ke mana (jangan 4 dan 5) buat akomodasi rtc ama clk
#define TX_PIN 33 // tar ganti ke mana (jangan 4 dan 5) buat akomodasi rtc ama miso

#define RX_PIN2 26
#define TX_PIN2 27

#define HSPI_MISO   12
#define HSPI_MOSI   13
#define HSPI_SCLK   14
#define HSPI_SS     15

static const int spiClk = 1000000; // 1 MHz

SPIClass * hspi = NULL;

long int prevMillis = 0;
RTC_DS1307 rtc;
int write_or_not = 0;
volatile int counter = 0;

int hari_lalu;
int bulan_lalu;
int tahun_lalu;

TaskHandle_t readStatusHandle = NULL;
TaskHandle_t receiveMessageHandle = NULL;
TaskHandle_t inverterStatusHandle = NULL;
TaskHandle_t pdu123Handle = NULL;
TaskHandle_t dcdcStateHandle = NULL;

//dibawah ini buat inverter, taro di inverter state
twai_message_t message_inverter_speed_rpm_temp;
twai_message_t message_inverter_voltage_current;
twai_message_t message_pdu_dcdc_input_output_voltage;
twai_message_t message_pdu_hv_precharge_ecu;
twai_message_t message_pdu_lv_voltage;
twai_message_t message_dcdc;
twai_message_t message_bms_hv_lv;
twai_message_t message_bms_hv_lv_state;
twai_message_t message_bms_cell1to4;
twai_message_t message_bms_cell5to6;
twai_message_t message_batt_charger;
twai_message_t message_batt_temp;
twai_message_t message_ignitionKey; 

const char* dcdc_bms_state_now_cstr;
const char* inverter_pdu_state_now_cstr;
const char* batt_temp_charger_now_cstr;
const char* dtc_p0xxx_cstr;
const char* dtc_c0xxx_cstr;
const char* dtc_b0xxx_cstr;
const char* dtc_all_str;

String speedy;
String rpms;
String temps;
String tegangan;
String arus;
String input_pdu_tegangan;
String output_pdu_tegangan;
String pdu_dcdc_arus;
String pdu_tegangan;
String pdu_hv;
String pdu_current;

String current;
String hv_input;
String lv_output;
String cvoltage1;
String cvoltage2;
String cvoltage3;
String cvoltage4;
String cvoltage5;
String cvoltage6;

String batt_voltage;
String batt_current;
String batt_temperature1;
String batt_temperature2;

String waktu;
String epoch_now;
String take_time(){
    int hari_sekarang;
    int bulan_sekarang;
    int tahun_sekarang;
    DateTime now = rtc.now();
    int hari = now.day();
    int bulan = now.month();
    int tahun = now.year();
    String pembatas = "-";
    String date = tahun + pembatas + bulan + pembatas + hari;

    int jam = now.hour();
    int menit = now.minute();
    int detik = now.second();
    String pembatas2 = ":";
    String waktu = jam + pembatas2 + menit + pembatas2 + detik;
    String pembatas3 = "T";
    String date_time = date + pembatas3 + waktu;

    if(hari != hari_lalu || bulan != bulan_lalu || tahun != tahun_lalu){
      hari_sekarang = hari;
      bulan_sekarang = bulan;
      tahun_sekarang = tahun;
      hari_lalu = hari_sekarang;
      bulan_lalu = bulan_sekarang;
      tahun_lalu = tahun_sekarang;
      DateTime startOfDay = DateTime(tahun, bulan, hari, 0, 0, 0);
      int epoch_unix = startOfDay.unixtime();
      String dcdc_bms_state_now = "/" + epoch_now + "_dcdcBMSState.txt";
      String inverter_pdu_state_now = "/" + epoch_now + "_inverterPDUState.txt";
      String batt_temp_state_now = "/" + epoch_now + "_battTempChargeState.txt";
      String dtc_all_now = "/" + epoch_now + "_dtc.txt";

      dcdc_bms_state_now_cstr = dcdc_bms_state_now.c_str();
      inverter_pdu_state_now_cstr = inverter_pdu_state_now.c_str();
      batt_temp_charger_now_cstr = batt_temp_state_now.c_str();
      dtc_all_str = dtc_all_now.c_str();
      // esp_restart();
      // esp_reset();      
    }

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

void write_to_sd_card(void *arg){
  while(1){
    long int currentMillis = millis();
    if(currentMillis - prevMillis >= 10000){
      waktu = take_time();
      speedy = waktu + speedy;
      tegangan = waktu + tegangan;
      arus = waktu + arus;
      rpms = waktu + rpms;
      temps = waktu + temps;
      input_pdu_tegangan = waktu + input_pdu_tegangan;
      output_pdu_tegangan = waktu + output_pdu_tegangan;
      pdu_dcdc_arus = waktu + pdu_dcdc_arus;
      pdu_hv = waktu + pdu_hv;
      pdu_current = waktu + pdu_current;
      pdu_tegangan = waktu + pdu_tegangan;
      current = waktu + current;
      hv_input = waktu + hv_input;
      lv_output = waktu + lv_output;
      cvoltage1 = waktu + cvoltage1;
      cvoltage2 = waktu + cvoltage2;
      cvoltage3 = waktu + cvoltage3;
      cvoltage4 = waktu + cvoltage4;
      cvoltage5 = waktu + cvoltage5;
      cvoltage6 = waktu + cvoltage6;
      if(write_or_not != 0){
        File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
        if (!file) {
          Serial.println("File not exist yet");
          writeFile(SD, inverter_pdu_state_now_cstr, "test\n");
        }
        else{
          Serial.println("File inverter voltage already exist");
        }
        // file.println(tegangan.isEmpty() ? "" : tegangan);
        tegangan.isEmpty() ? printf("\nKosong 1") : file.println(tegangan);
        arus.isEmpty() ? printf("\nKosong 2") : file.println(arus);
        speedy.isEmpty() ? printf("\nKosong 3") : file.println(speedy);
        rpms.isEmpty() ? printf("\nKosong 4") : file.println(rpms);
        temps.isEmpty() ? printf("\nKosong 5") : file.println(temps);
        input_pdu_tegangan.isEmpty() ? printf("\nKosong 6") : file.println(input_pdu_tegangan);
        output_pdu_tegangan.isEmpty() ? printf("\nKosong 7") : file.println(output_pdu_tegangan);
        pdu_dcdc_arus.isEmpty() ? printf("\nKosong 8") : file.println(pdu_dcdc_arus);
        pdu_hv.isEmpty() ? printf("\nKosong 9") : file.println(pdu_hv);
        pdu_current.isEmpty() ? printf("\nKosong 10") : file.println(pdu_current);
        pdu_tegangan.isEmpty() ? printf("\nKosong 11") : file.println(pdu_tegangan);
        file.close();
        Serial.println("inverter successfully added");

        file = SD.open(dcdc_bms_state_now_cstr, FILE_APPEND);
        if (!file) {
          Serial.println("File not exist yet");
          writeFile(SD, dcdc_bms_state_now_cstr, "test\n");
        }
        else{
          Serial.println("File dcdc bms already exist");
        }
        // file.println(current);
        current.isEmpty() ? printf("\nKosong 12") : file.println(current);
        hv_input.isEmpty() ? printf("\nKosong 13") : file.println(hv_input);
        lv_output.isEmpty() ? printf("\nKosong 14") : file.println(lv_output);
        cvoltage1.isEmpty() ? printf("\nKosong 15") : file.println(cvoltage1);
        cvoltage2.isEmpty() ? printf("\nKosong 16") : file.println(cvoltage2);
        cvoltage3.isEmpty() ? printf("\nKosong 17") : file.println(cvoltage3);
        cvoltage4.isEmpty() ? printf("\nKosong 18") : file.println(cvoltage4);
        cvoltage5.isEmpty() ? printf("\nKosong 19") : file.println(cvoltage5);
        cvoltage6.isEmpty() ? printf("\nKosong 20") : file.println(cvoltage6);
        file.close();
        Serial.println("dcdc bms successfully added");

        // file = SD.open(batt_temp_charger_now_cstr, FILE_APPEND);
        // if (!file) {
        //   Serial.println("File not exist yet");
        //   writeFile(SD, batt_temp_charger_now_cstr, "test\n");
        // }
        // else{
        //   Serial.println("File batt temp charge already exist");
        // }
        // file.println(batt_voltage);
        // file.println(batt_current);
        // file.println(batt_temperature1);
        // file.println(batt_temperature2);
        // file.close();
        // Serial.println("batt temp charge successfully added");
        tegangan = "";
        arus = "";
        speedy = "";
        rpms = "";
        temps = "";
        input_pdu_tegangan = "";
        output_pdu_tegangan = "";
        pdu_dcdc_arus = "";
        pdu_current = "";
        pdu_tegangan = "";
        current = "";
        hv_input = "";
        lv_output = "";
        cvoltage1 = "";
        cvoltage2 = "";
        cvoltage3 = "";
        cvoltage4 = "";
        cvoltage5 = "";
        cvoltage6 = "";
        batt_voltage = "";
        batt_current = "";
        batt_temperature1 = "";
        batt_temperature2 = "";
        prevMillis = currentMillis;
        printf("\nWaktu millis: %d", currentMillis);
        hspiConfirm();
        Serial.println(waktu);
      }      
    }
    vTaskDelay(500/portTICK_PERIOD_MS);
  }
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
    int real_speed;
    int real_rpm;
    uint16_t engine_temp;
    uint16_t inverter_voltage;
    uint16_t inverter_current;
    uint16_t pdu_input_voltage;
    uint16_t pdu_output_voltage;
    uint16_t pdu_dcdc_current;
    uint16_t pdu_voltage;
    long int currentMillis = millis();
    // if(currentMillis - prevMillis >= 7000){
      if(write_or_not == 1){
        // waktu = take_time();
        if(twai_receive(&message_inverter_speed_rpm_temp, pdMS_TO_TICKS(1000)) == ESP_OK){
              if(message_inverter_speed_rpm_temp.identifier == 0x0CF00401){
                printf("\nID Inverter: %x", message_inverter_speed_rpm_temp.identifier);        
                uint8_t speed_msb = message_inverter_speed_rpm_temp.data[2];
                uint8_t speed_lsb = message_inverter_speed_rpm_temp.data[1];
                uint16_t engine_speed = (speed_msb << 8) | speed_lsb;
                real_speed = engine_speed * 0.125;
                speedy = String("\t") + String(real_speed) + "\t\t" + "engine_speed";
                // Serial.println(waktu);

                uint8_t rpm_msb = message_inverter_speed_rpm_temp.data[4];
                uint8_t rpm_lsb = message_inverter_speed_rpm_temp.data[3];
                uint16_t engine_rpm = (rpm_msb << 8) | rpm_lsb;
                real_rpm = engine_rpm * 0.125;
                rpms = String("\t") + String(real_rpm) + "\t\t" + "engine_rpm";

                uint8_t temp_msb = message_inverter_speed_rpm_temp.data[6];
                uint8_t temp_lsb = message_inverter_speed_rpm_temp.data[5];
                engine_temp = (temp_msb << 8) | temp_lsb;
                temps = String("\t") + String(engine_temp) + "\t\t" + "engine_temp";
                // if(currentMillis - prevMillis >= 7000){
                //   File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
                //   if (!file) {
                //     Serial.println("File not exist yet");
                //   }
                //   else{
                //     Serial.println("File engine speed already exist");
                //   }
                //   file.println(speedy);
                //   file.println(rpms);
                //   file.println(temps);
                //   file.close();
                //   Serial.println("engine speeds successfully added");
                // }
            }
          }
        if(twai_receive(&message_inverter_voltage_current, pdMS_TO_TICKS(1000)) == ESP_OK){
              if(message_inverter_voltage_current.identifier == 0x0CF00402){
                printf("\nID Inverter: %x", message_inverter_voltage_current.identifier);        
                uint8_t voltage_msb = message_inverter_voltage_current.data[1];
                uint8_t voltage_lsb =message_inverter_voltage_current.data[0];
                inverter_voltage = (voltage_msb << 8) | voltage_lsb;
                // int real_speed = engine_speed * 0.125;
                tegangan = String("\t") + inverter_voltage + "\t\t" + "inverter_voltage";

                uint8_t current_msb = message_inverter_voltage_current.data[3];
                uint8_t current_lsb = message_inverter_voltage_current.data[2];
                inverter_current = (current_msb << 8) | current_lsb;
                // int real_rpm = engine_rpm * 0.125;
                arus = String("\t") + inverter_current + "\t\t" + "inverter_current";
            }
          }
          if(twai_receive(&message_pdu_dcdc_input_output_voltage, pdMS_TO_TICKS(1000)) == ESP_OK){
              if(message_inverter_speed_rpm_temp.identifier == 0x18FE8D02){
                printf("\nID Inverter: %x", message_pdu_dcdc_input_output_voltage.identifier);        
                uint8_t input_voltage_msb = message_pdu_dcdc_input_output_voltage.data[2];
                uint8_t input_voltage_lsb = message_pdu_dcdc_input_output_voltage.data[1];
                pdu_input_voltage = (input_voltage_msb << 8) | input_voltage_lsb;
                // int real_speed = engine_speed * 0.125;
                input_pdu_tegangan = String("\t") + pdu_input_voltage + "\t\t" + "pdu_input_voltage";

                uint8_t output_voltage_msb = message_pdu_dcdc_input_output_voltage.data[4];
                uint8_t output_voltage_lsb = message_pdu_dcdc_input_output_voltage.data[3];
                pdu_output_voltage = (output_voltage_msb << 8) | output_voltage_lsb;
                // int real_speed = engine_speed * 0.125;
                output_pdu_tegangan = String("\t") + pdu_output_voltage + "\t\t" + "pdu_output_voltage";

                uint8_t pdu_dcdc_current_msb = message_pdu_dcdc_input_output_voltage.data[6];
                uint8_t pdu_dcdc_current_lsb = message_pdu_dcdc_input_output_voltage.data[5];
                pdu_dcdc_current = (pdu_dcdc_current_msb << 8) | pdu_dcdc_current_lsb;
                // int real_rpm = engine_rpm * 0.125;
                pdu_dcdc_arus = String("\t") + pdu_dcdc_current + "\t\t" + "pdu_dcdc_current";
                // if(currentMillis - prevMillis >= 7000){
                //   File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
                //   if (!file) {
                //     Serial.println("File not exist yet");
                //   }
                //   else{
                //     Serial.println("File inverter voltage already exist");
                //   }
                //   file.println(input_pdu_tegangan);
                //   file.println(output_pdu_tegangan);
                //   file.println(pdu_dcdc_arus);
                //   file.close();
                //   Serial.println("pdu current successfully added");
                // }
            }
          }
          if(twai_receive(&message_pdu_dcdc_input_output_voltage, pdMS_TO_TICKS(1000)) == ESP_OK){
              if(message_pdu_dcdc_input_output_voltage.identifier == 0x18FE8D01){
                printf("\nID Inverter: %x", message_pdu_dcdc_input_output_voltage.identifier);        
                // uint8_t input_voltage_msb = message_pdu_hv_precharge_ecu.data[2];
                // uint8_t input_voltage_lsb = message_pdu_hv_precharge_ecu.data[1];
                // pdu_input_voltage = (input_voltage_msb << 8) | input_voltage_lsb;
                // // int real_speed = engine_speed * 0.125;
                // String input_pdu_tegangan = waktu + "\t" + pdu_input_voltage + "\t\t" + "pdu_input_voltage";

                uint8_t output_voltage_msb = message_pdu_dcdc_input_output_voltage.data[4];
                uint8_t output_voltage_lsb = message_pdu_dcdc_input_output_voltage.data[3];
                pdu_output_voltage = (output_voltage_msb << 8) | output_voltage_lsb;
                // int real_speed = engine_speed * 0.125;
                pdu_hv = String("\t") + pdu_output_voltage + "\t\t" + "pdu_output_voltage";

                uint8_t pdu_dcdc_current_msb = message_pdu_dcdc_input_output_voltage.data[6];
                uint8_t pdu_dcdc_current_lsb = message_pdu_dcdc_input_output_voltage.data[5];
                pdu_dcdc_current = (pdu_dcdc_current_msb << 8) | pdu_dcdc_current_lsb;
                // int real_rpm = engine_rpm * 0.125;
                pdu_current = String("\t") + pdu_dcdc_current + "\t\t" + "pdu_dcdc_current";
                // if(currentMillis - prevMillis >= 7000){
                //   File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
                //   if (!file) {
                //     Serial.println("File not exist yet");
                //   }
                //   else{
                //     Serial.println("File inverter voltage already exist");
                //   }
                //   file.println(input_pdu_tegangan);
                //   file.println(output_pdu_tegangan);
                //   file.println(pdu_dcdc_arus);
                //   file.close();
                //   Serial.println("PDU IO Successfully added");
                // }                
            }
          }
          if(twai_receive(&message_pdu_hv_precharge_ecu, pdMS_TO_TICKS(1000)) == ESP_OK){
              if(message_pdu_hv_precharge_ecu.identifier == 0x18FE8D03){
                printf("\nID Inverter: %x", message_pdu_hv_precharge_ecu.identifier);        
                uint8_t pdu_voltage_msb = message_pdu_hv_precharge_ecu.data[2];
                uint8_t pdu_voltage_lsb = message_pdu_hv_precharge_ecu.data[1];
                pdu_voltage = (pdu_voltage_msb << 8) | pdu_voltage_lsb;
                // int real_speed = engine_speed * 0.125;
                pdu_tegangan = String("\t") + pdu_voltage + "\t\t" + "pdu_voltage";
                // if(currentMillis - prevMillis >= 7000){
                //   File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
                //   if (!file) {
                //     Serial.println("File not exist yet");
                //   }
                //   else{
                //     Serial.println("File inverter voltage already exist");
                //   }
                //   file.println(pdu_tegangan);
                //   file.close();
                //   Serial.println("PDU Voltage successfully added");
                // }
            }
          }
          // if(currentMillis - prevMillis >= 7000){
          //   // printf("\nCurrent millis ke-2: %d", millis());
          //   if(speedy != ""){
          //     File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
          //     if (!file) {
          //       Serial.println("File not exist yet");
          //     }
          //     else{
          //       Serial.println("File inverter voltage already exist");
          //     }
          //     file.println(tegangan);
          //     file.println(arus);
          //     file.println(speedy);
          //     file.println(rpms);
          //     file.println(temps);
          //     file.println(input_pdu_tegangan);
          //     file.println(output_pdu_tegangan);
          //     file.println(pdu_dcdc_arus);
          //     file.println(pdu_hv);
          //     file.println(pdu_current);
          //     file.println(pdu_tegangan);
          //     file.close();
          //     Serial.println("inverter successfully added");
          //     prevMillis = currentMillis;
          //   }
          // }
        // printf("\nWaktu kemakan buat inverter: %d", millis());
        // Serial.println("This task watermark: " + String(uxTaskGetStackHighWaterMark(NULL)) + " bytes");
      // }
      // else{
      //   Serial.println("Don't do anything");
      // }
    }
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}

void read_dcdc_bms_status(void *arg){
  while(1){
    uint16_t dcdc_current;
    uint16_t dcdc_hv_input;
    uint16_t dcdc_lv_output;
    uint16_t cell_voltage1;  
    uint16_t cell_voltage2;
    uint16_t cell_voltage3;
    uint16_t cell_voltage4;
    uint16_t cell_voltage5;
    uint16_t cell_voltage6;  
    long int currentMillis = millis();
    // if(currentMillis - prevMillis >= 7000){
      if(write_or_not == 1){
        if(twai_receive(&message_dcdc, pdMS_TO_TICKS(1000)) == ESP_OK){
              if(message_dcdc.identifier == 0x18F11401){
                printf("\nID Inverter: %x", message_dcdc.identifier);        
                uint8_t dcdc_current_msb = message_dcdc.data[3];
                uint8_t dcdc_current_lsb = message_dcdc.data[2];
                dcdc_current = (dcdc_current_msb << 8) | dcdc_current_lsb;
                current = "\t" + String(dcdc_current) + "\t\t" + "dcdc_current";

                uint8_t dcdc_hv_input_msb = message_dcdc.data[5];
                uint8_t dcdc_hv_input_lsb = message_dcdc.data[4];
                dcdc_hv_input = (dcdc_hv_input_msb << 8) | dcdc_hv_input_lsb;
                hv_input = "\t" + String(dcdc_hv_input) + "\t\t" + "dcdc_hv_input";

                uint8_t dcdc_lv_output_msb = message_dcdc.data[7];
                uint8_t dcdc_lv_output_lsb = message_dcdc.data[6];
                dcdc_lv_output = (dcdc_lv_output_msb << 8) | dcdc_lv_output_lsb;
                lv_output = "\t" + String(dcdc_lv_output) + "\t\t" + "dcdc_lv_output";
                // if(currentMillis - prevMillis >= 7000){
                //   File file = SD.open(dcdc_bms_state_now_cstr, FILE_APPEND);
                //   if (!file) {
                //     Serial.println("File not exist yet");
                //   }
                //   else{
                //     Serial.println("File dcdc bms already exist");
                //   }
                //   file.println(current);
                //   file.println(hv_input);
                //   file.println(lv_output);
                //   file.close();
                //   Serial.println("dcdc current hv lv successfully added");
                // }
            }
          }
          if(twai_receive(&message_bms_cell1to4, pdMS_TO_TICKS(1000)) == ESP_OK){
              if(message_bms_cell1to4.identifier == 0x10F51403){
                printf("\nID Inverter: %x", message_bms_cell1to4.identifier);        
                uint8_t cell_voltage1_msb = message_bms_cell1to4.data[1];
                uint8_t cell_voltage1_lsb = message_bms_cell1to4.data[0];
                cell_voltage1 = (cell_voltage1_msb << 8) | cell_voltage1_lsb;
                cvoltage1 = String("\t") + "1" + "\t" + String(cell_voltage1) + "\t\t" + "cell_voltage1";

                uint8_t cell_voltage2_msb = message_bms_cell1to4.data[3];
                uint8_t cell_voltage2_lsb = message_bms_cell1to4.data[2];
                cell_voltage2 = (cell_voltage2_msb << 8) | cell_voltage2_lsb;
                cvoltage2 = String("\t") + "2" + "\t" + String(cell_voltage2) + "\t\t" + "cell_voltage2";

                uint8_t cell_voltage3_msb = message_bms_cell1to4.data[5];
                uint8_t cell_voltage3_lsb = message_bms_cell1to4.data[4];
                cell_voltage3 = (cell_voltage3_msb << 8) | cell_voltage3_lsb;
                cvoltage3 = String("\t") + "3" + "\t" + String(cell_voltage3) + "\t\t" + "cell_voltage3";

                uint8_t cell_voltage4_msb = message_bms_cell1to4.data[7];
                uint8_t cell_voltage4_lsb = message_bms_cell1to4.data[6];
                cell_voltage4 = (cell_voltage4_msb << 8) | cell_voltage4_lsb;
                cvoltage4 = String("\t") + "4" + "\t" + String(cell_voltage4) + "\t\t" + "cell_voltage4";
                // if(currentMillis - prevMillis >= 7000){
                // File file = SD.open(dcdc_bms_state_now_cstr, FILE_APPEND);
                //   if (!file) {
                //     Serial.println("File not exist yet");
                //   }
                //   else{
                //     Serial.println("File dcdc bms already exist");
                //   }
                //   file.println(cvoltage1);
                //   file.println(cvoltage2);
                //   file.println(cvoltage3);
                //   file.println(cvoltage4);
                //   file.close();
                //   Serial.print("cell voltage data successfully added");
                // }
            }
          }
          if(twai_receive(&message_bms_cell5to6, pdMS_TO_TICKS(1000)) == ESP_OK){
              if(message_bms_cell5to6.identifier == 0x10F51404){
                printf("\nID Inverter: %x", message_bms_cell5to6.identifier);        
                uint8_t cell_voltage5_msb = message_bms_cell5to6.data[1];
                uint8_t cell_voltage5_lsb = message_bms_cell5to6.data[0];
                cell_voltage5 = (cell_voltage5_msb << 8) | cell_voltage5_lsb;
                String cvoltage5 = String("\t") + "5" + "\t" + String(cell_voltage5) + "\t\t" + "cell_voltage5";

                uint8_t cell_voltage6_msb = message_bms_cell5to6.data[3];
                uint8_t cell_voltage6_lsb = message_bms_cell5to6.data[2];
                cell_voltage6 = (cell_voltage6_msb << 8) | cell_voltage6_lsb;
                String cvoltage6 = String("\t") + "6" + "\t" + String(cell_voltage6) + "\t\t" + "cell_voltage6";

                // if(currentMillis - prevMillis >= 7000){
                //   File file = SD.open(dcdc_bms_state_now_cstr, FILE_APPEND);
                //   if (!file) {
                //     Serial.println("File not exist yet");
                //   }
                //   else{
                //     Serial.println("File dcdc bms already exist");
                //   }
                //   file.println(cvoltage5);
                //   file.println(cvoltage6);
                //   file.close();
                //   Serial.println("voltage cells successfully added");
                // }
            }
          }
      }
    vTaskDelay(1000/portTICK_PERIOD_MS);    
  }
}

void battTempChargeStatus(void *arg){
  while(1){
    uint16_t batt_charge_voltage;
    uint16_t batt_charge_current;
    int batt_temp1;
    int batt_temp2;  
    long int currentMillis = millis();
    // if(currentMillis - prevMillis >= 7000){
      if(write_or_not == 1){
        if(twai_receive(&message_batt_charger, pdMS_TO_TICKS(1000)) == ESP_OK){
              if(message_batt_charger.identifier == 0x00FD15){
                printf("\nID Inverter: %x", message_batt_charger.identifier);        
                uint8_t batt_charge_voltage_msb = message_batt_charger.data[3];
                uint8_t batt_charge_voltage_lsb = message_batt_charger.data[2];
                batt_charge_voltage = (batt_charge_voltage_msb << 8) | batt_charge_voltage_lsb;
                batt_voltage = String("\t") + String(batt_charge_voltage) + "\t\t" + "batt_charge_voltage";

                uint8_t batt_charge_current_msb = message_batt_charger.data[5];
                uint8_t batt_charge_current_lsb = message_batt_charger.data[4];
                batt_charge_current = (batt_charge_current_msb << 8) | batt_charge_current_lsb;
                batt_current = String("\t") + String(batt_charge_current) + "\t\t" + "batt_charge_current";

            //     File file = SD.open(batt_temp_charger_now_cstr, FILE_APPEND);
            //     if (!file) {
            //       Serial.println("File not exist yet");
            //     }
            //     else{
            //       Serial.println("File batt temp charge already exist");
            //     }
            //     file.println(batt_voltage);
            //     file.println(batt_current);
            //     file.close();
            // }
          }

          if(twai_receive(&message_batt_temp, pdMS_TO_TICKS(1000)) == ESP_OK){
              if(message_batt_temp.identifier == 0x00FE50){
                printf("\nID Inverter: %x", message_batt_temp.identifier);        
                batt_temp1 = message_batt_temp.data[0];
                String batt_temperature1 = String("\t") + String(batt_temp1) + "\t\t" + "batt_temp1";

                batt_temp2 = message_batt_temp.data[1];
                String batt_temperature2 = String("\t") + String(batt_temp2) + "\t\t" + "batt_temp2";
                
                // File file = SD.open(batt_temp_charger_now_cstr, FILE_APPEND);
                // if (!file) {
                //   Serial.println("File not exist yet");
                // }
                // else{
                //   Serial.println("File batt temp charge already exist");
                // }
                // file.println(batt_temperature1);
                // file.println(batt_temperature2);
                // file.close();
            }
          }

          // if(currentMillis - prevMillis >= 7000){
          //   if(batt_voltage != ""){
          //     File file = SD.open(batt_temp_charger_now_cstr, FILE_APPEND);
          //     if (!file) {
          //       Serial.println("File not exist yet");
          //     }
          //     else{
          //       Serial.println("File batt temp charge already exist");
          //     }
          //     file.println(batt_voltage);
          //     file.println(batt_current);
          //     file.println(batt_temperature1);
          //     file.println(batt_temperature2);
          //     file.close();
          //     prevMillis = currentMillis;
          //     }
          //   }
          }
      //     }
      //   prevMillis = currentMillis;
      //   printf("\nWaktu kemakan buat battery: %d", millis());
      // }
    // else{
    //   Serial.println("Don't do anything either");
    //   }
    }
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}

void ignitionKey(void *arg){
  while(1){
    long int currentMillis = millis();
    // if(currentMillis - prevMillis >= 3000){
      if(twai_receive(&message_ignitionKey, pdMS_TO_TICKS(1000)) == ESP_OK){
            if(message_ignitionKey.identifier == 0x00FD15){
              printf("\nID igniton key ID: %x", message_ignitionKey.identifier);        
              int ignition_key = message_ignitionKey.data[0];
              // if(currentMillis - prevMillis >= 5000){
                if(ignition_key == 0){
                  write_or_not = 0;
                  hspiYes();
                }
                else{
                  write_or_not = 1;
                  hspiNo();
                }
              //   prevMillis = currentMillis;
              // }
          }
        }
    // }
        // SD.end();
        // File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
        // if (!file) {
        //   Serial.println("File not exist yet");
        // }
        // else{
        //   Serial.println("File inverter voltage already exist");
        // }
        hspiYes();
        // hspiNo();
    vTaskDelay(1000/portTICK_PERIOD_MS); 
  }
}

void hspiYes() {
  byte stuff = 0b00000001;
  
  hspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(HSPI_SS, LOW);
  hspi->transfer(stuff);
  Serial.println("Data dikirim");
  digitalWrite(HSPI_SS, HIGH); //SS low untuk start data transfer, SS high untuk stop data transfer
  hspi->endTransaction();
}

void hspiNo() {
  byte stuff = 0b00000000;
  
  hspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(HSPI_SS, LOW);
  hspi->transfer(stuff);
  digitalWrite(HSPI_SS, HIGH); //SS low untuk start data transfer, SS high untuk stop data transfer
  hspi->endTransaction();
}

void hspiConfirm() {
  byte stuff = 200;
  
  hspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(HSPI_SS, LOW);
  hspi->transfer(stuff);
  digitalWrite(HSPI_SS, HIGH); //SS low untuk start data transfer, SS high untuk stop data transfer
  hspi->endTransaction();
}

void counter_inc(){
  counter += 1;    
}

void counter_reset(){
  counter = 0;
}

void setup(){
  // take_time();
  byte datetimeBytes[7];
  if(!rtc.begin()){
    Serial.println("RTC Tidak bisa start Sat!");
  }
  // rtc.adjust(DateTime(2024, 3, 17, 17, 12, 0));
  DateTime now = rtc.now();
  int hari = now.day();
  hari_lalu = hari;
  String day = String(hari);

  int bulan = now.month();
  bulan_lalu = bulan;
  String month = String(bulan);

  int tahun = now.year();
  tahun_lalu = tahun;
  String year = String(tahun);

  // byte tahun_ini = tahun;
  byte bulan_ini = bulan;
  byte hari_ini = hari;
  byte jam = now.hour();
  byte menit = now.minute();
  byte detik = now.second();

  datetimeBytes[0] = tahun >> 8;   // High byte of year
  datetimeBytes[1] = tahun & 0xFF; // Low byte of year
  datetimeBytes[2] = bulan_ini;
  datetimeBytes[3] = hari_ini;
  datetimeBytes[4] = jam;
  datetimeBytes[5] = menit;
  datetimeBytes[6] = detik;

  unsigned long combined_data = ((unsigned long)tahun << 48) | ((unsigned long)bulan << 40) | ((unsigned long)hari << 32) | ((unsigned long)jam << 24) | ((unsigned long)menit << 16) | (detik << 8);
  DateTime startOfDay = DateTime(tahun, bulan, hari, 0, 0, 0);
  int epoch_unix = startOfDay.unixtime();
  epoch_now = String(epoch_unix);
  printf("\n%d", epoch_unix);
  hspi = new SPIClass(HSPI);

  // pinMode(5, OUTPUT);
  // digitalWrite(5, LOW);
  // digitalWrite(5, HIGH);
  // delay(2000);
  Serial.begin(115200);
  // Serial.println(day);
  // for(int i = 0;i < day.length();i++){
  //   Serial.println(bufferDay[i], HEX);
  // }
  twai_setup_and_install_for_send();
  // delay(2000); //to ensure datetime is sent
  delay(2000);
  hspi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_SS);  
  pinMode(HSPI_SS, OUTPUT); //HSPI SS
  hspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(HSPI_SS, LOW); // SS low to start data transfer
  
  // Send the combined long integer data byte by byte
  for (int i = 0; i < 7; i++) {
    // byte byteToSend = (combined_data >> (i * 8)) & 0xFF; // Extract each byte
    hspi->transfer(datetimeBytes[i]); // Send the byte over SPI
    Serial.println("Sending data");
  }
  Serial.println(datetimeBytes[2]);
  
  digitalWrite(HSPI_SS, HIGH); // SS high to stop data transfer
  hspi->endTransaction();
  delay(7000); //used to ensure if slave has accessed the sd card
  if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
    // attachInterrupt(0, counter_inc, CHANGE);
    // Serial.println(counter);
    // if(counter == 5){
    //   return;
    // }
    esp_restart();
    // return;
  }
  // else{
  //   // counter = 0;
  //   attachInterrupt(0, counter_reset, CHANGE);
  // }
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

  String dcdc_bms_state_now = "/" + epoch_now + "_dcdcBMSState.txt";
  String inverter_pdu_state_now = "/" + epoch_now + "_inverterPDUState.txt";
  String batt_temp_state_now = "/" + epoch_now + "_battTempChargeState.txt";
  // String dtc_p0xxx = "/" + epoch_now + "_dtc_p0xxx.txt";
  // String dtc_b0xxx = "/" + epoch_now + "_dtc_b0xxx.txt";
  // String dtc_c0xxx = "/" + epoch_now + "_dtc_c0xxx.txt";
  String dtc_all_now = "/" + epoch_now + "_dtc.txt";

  Serial.println(inverter_pdu_state_now);

  dcdc_bms_state_now_cstr = dcdc_bms_state_now.c_str();
  inverter_pdu_state_now_cstr = inverter_pdu_state_now.c_str();
  batt_temp_charger_now_cstr = batt_temp_state_now.c_str();
  // dtc_p0xxx_cstr = dtc_p0xxx.c_str();
  // dtc_b0xxx_cstr = dtc_b0xxx.c_str();
  // dtc_c0xxx_cstr = dtc_c0xxx.c_str();
  dtc_all_str = dtc_all_now.c_str();

  // printf("\n");
  // printf(inverter_pdu_state_now_cstr);
  printf(dcdc_bms_state_now_cstr);

  File file = SD.open(inverter_pdu_state_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File inverter and pdu state for today does not exist yet");
    writeFile(SD, inverter_pdu_state_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File inverter and pdu state already exist");
  }
  file.close();

  file = SD.open(dcdc_bms_state_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File dcdc and bms state for today does not exist yet");
    writeFile(SD, dcdc_bms_state_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File dcdc and bms state already exist");
  }
  file.close();

  file = SD.open(batt_temp_charger_now_cstr, FILE_APPEND);
  if(!file){
    Serial.println("File batt temp and charger state for today does not exist yet");
    writeFile(SD, batt_temp_charger_now_cstr, "test\r\n");
  }
  else{
    Serial.println("File batt temp and charger state already exist");
  }
  file.close();

  // file = SD.open(dtc_p0xxx_cstr, FILE_APPEND);
  // if(!file){
  //   Serial.println("File DTC powertrain not exist yet");
  //   writeFile(SD, dtc_p0xxx_cstr, "\n");    
  // }
  // else{
  //   Serial.println("File DTC powertrain already exist");
  // }
  // file.close();

  // file = SD.open(dtc_c0xxx_cstr, FILE_APPEND);
  // if(!file){
  //   Serial.println("File DTC chassis not exist yet");
  //   writeFile(SD, dtc_c0xxx_cstr, "\n");    
  // }
  // else{
  //   Serial.println("File DTC chassis already exist");
  // }
  // file.close();

  // file = SD.open(dtc_b0xxx_cstr, FILE_APPEND);
  // if(!file){
  //   Serial.println("File DTC body not exist yet");
  //   writeFile(SD, dtc_b0xxx_cstr, "\n");    
  // }
  // else{
  //   Serial.println("File DTC body already exist");
  // }
  // file.close();

  file = SD.open(dtc_all_str, FILE_APPEND);
  if(!file){
    Serial.println("File DTC not exist yet");
    writeFile(SD, dtc_all_str, "\n");
  }
  file.close();
  // SD.end();
  // SPI.end();
  // file = SD.open(dtc_b0xxx_cstr, FILE_APPEND);
  // if(!file){
  //   Serial.println("There's something wrong");
  // }

  Serial.println("[APP] Free memory: " + String(esp_get_free_heap_size()) + " bytes");

  xTaskCreatePinnedToCore(read_inverter_status, "inverterStatus", 3000, NULL, 3, &inverterStatusHandle, 1);
  xTaskCreatePinnedToCore(read_dcdc_bms_status, "bmsStatus", 3000, NULL, 4, NULL, 0);
  xTaskCreatePinnedToCore(battTempChargeStatus, "battStatus", 2500, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(write_to_sd_card, "tulis_sd_card", 5000, NULL, 4, NULL, 0);
  xTaskCreatePinnedToCore(ignitionKey, "ignitionKey", 2500, NULL, 2, NULL, 1);
  vTaskDelete(NULL);

  //inget, priority makin tinggi, resource pool ke tugas itu makin banyak
}

void loop()
{

}
