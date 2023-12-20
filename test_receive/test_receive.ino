#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdio.h"
#include "stdlib.h"
#include "driver/twai.h"
#include "SD.h"
#include "SPI.h"
#include "FS.h"
#include "BluetoothSerial.h"

#define OBD2_REQUEST_ID 0x7DF
#define OBD2_RESPONSE_ID 0x7E8
#define OBD2_REQUEST_EXTENDED_ID 0x18DB33F1

int prevMillis = 0;

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
    twai_general_config_t g_config = {
        .mode = TWAI_MODE_NORMAL,
        .tx_io = GPIO_NUM_21,
        .rx_io = GPIO_NUM_22,
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 5,
        .rx_queue_len = 5,
        .alerts_enabled = TWAI_ALERT_NONE,
        .clkout_divider = 0
    };

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

void twai_setup_and_install_for_receive(){
    //Initialize configuration structures using macro initializers
    twai_general_config_t g_config = {
        .mode = TWAI_MODE_NORMAL,
        .tx_io = GPIO_NUM_13,
        .rx_io = GPIO_NUM_14,
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 5,
        .rx_queue_len = 5,
        .alerts_enabled = TWAI_ALERT_NONE,
        .clkout_divider = 0
    };

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

void read_inverter_status(twai_message_t *message){
  if(twai_receive(message, pdMS_TO_TICKS(1000)) == ESP_OK){
    switch(message->identifier){
      case 0x0CF00402:
        File file = SD.open("/inverter_status.txt", FILE_APPEND);
        if (!file) {
          Serial.println("File not exist yet");
        }
        else{
          Serial.println("File inverter status already exist");
        }
        uint8_t status = message->data[0];
        uint8_t mock_value[8] = {02, 43, status, 00, 00, 00, 00, 00};
        for(int i = 0; i < message->data_length_code; i++){
          printf("%x\t", mock_value[i]);
          //  // Append data to the file
          if (file.print(String(mock_value[i]))) {
            Serial.println("Message inverter appended");
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
  }
  }
}

void read_pdu_state1(twai_message_t *message){
  if(twai_receive(message, pdMS_TO_TICKS(1000)) == ESP_OK){
    switch(message->identifier){
      case 0x10F51402:
        File file = SD.open("/pdu_status.txt", FILE_APPEND);
        if (!file) {
          Serial.println("File does not exist yet");
        }
        else{
          Serial.println("File pdu status already exist");
        }
        // uint8_t status_bms_msb = message->data[1];
        // uint8_t status_bms_lsb = message->data[0];
        for(int i = 0; i < message->data_length_code; i++){
          printf("%x\t", message->data[i]);
          //  // Append data to the file
           if (file.print(String(message->data[i]))) {
              Serial.println("Message pdu1 appended");
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
  }
  }  
}

void read_pdu_state2(twai_message_t *message){
  if(twai_receive(message, pdMS_TO_TICKS(1000)) == ESP_OK){
    switch(message->identifier){
      case 0x18FE8D02:
        File file = SD.open("/pdu_state", FILE_APPEND);
        if(!file){
          Serial.println("File does not exist yet");
        }
        else{
          Serial.println("File pdu state already exist");
        }
        uint8_t dcdc_state = message->data[0];
        uint8_t mock_value[8] = {01, 43, dcdc_state, 00, 00, 00, 00, 00};
        for(int i = 0; i < message->data_length_code; i++){
          printf("%x\t", mock_value[i]);
          //  // Append data to the file
           if (file.print(String(mock_value[i]))) {
              Serial.println("Message pdu2 appended");
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
  }
  }
}

void read_pdu_state3(twai_message_t *message){
  if(twai_receive(message, pdMS_TO_TICKS(1000)) == ESP_OK){
    switch(message->identifier){
      case 0x18FE8D03:
        File file = SD.open("/pdu_state", FILE_APPEND);
        if(!file){
          Serial.println("File does not exist");
        }
        else{
          Serial.println("File pdu state already exist");
        }
        uint8_t lv_state = message->data[0];
        uint8_t mock_value[8] = {01, 43, 00, lv_state, 00, 00, 00, 00};
        for(int i = 0; i < message->data_length_code; i++){
          printf("%x\t", mock_value[i]);
          //  // Append data to the file
           if (file.print(String(mock_value[i]))) {
              Serial.println("Message pdu3 appended");
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
  }
  }
}

void read_dcdc_state(twai_message_t *message){
  if(twai_receive(message, pdMS_TO_TICKS(1000)) == ESP_OK){
    switch(message->identifier){
      case 0x18F11401:
        File file = SD.open("/dcdc_state", FILE_APPEND);
        if(!file){
          Serial.println("File does not exist yet");
        }
        else{
          Serial.println("File dcdc state already exist");          
        }
        for(int i = 0; i < message->data_length_code; i++){
          printf("%x\t", message->data[i]);
          //  // Append data to the file
           if (file.print(String(message->data[i]))) {
              Serial.println("Message dcdc appended");
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
  }
  }
}

void read_status(twai_message_t *message){
  if(twai_receive(message, pdMS_TO_TICKS(1000)) == ESP_OK){
    switch(message->identifier){
      case 0x0CF00402:
      {
        File file = SD.open("/inverter_status.txt", FILE_APPEND);
        if (!file) {
          Serial.println("File not exist yet");
        }
        else{
          Serial.println("File inverter status already exist");
        }
        uint8_t status = message->data[0];
        uint8_t mock_value[8] = {02, 43, status, 00, 00, 00, 00, 00};
        for(int i = 0; i < message->data_length_code; i++){
          printf("%x\t", mock_value[i]);
          //  // Append data to the file
          if (file.print(String(mock_value[i]))) {
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
        break;
      }

      case 0x10F51402:
      {
        File file = SD.open("/bms_status.txt", FILE_APPEND);
        if (!file) {
          Serial.println("File does not exist yet");
        }
        else{
          Serial.println("File bms status already exist");
        }
        // uint8_t status_bms_msb = message->data[1];
        // uint8_t status_bms_lsb = message->data[0];
        for(int i = 0; i < message->data_length_code; i++){
          printf("%x\t", message->data[i]);
          //  // Append data to the file
           if (file.print(String(message->data[i]))) {
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
        break;
      }

      case 0x18FE8D01:
      {
        File file = SD.open("/pdu_state", FILE_APPEND);
        if(!file){
          Serial.println("File does not exist yet");
        }
        else{
          Serial.println("File pdu state already exist");
        }
        uint8_t hv_state = message->data[0];
        uint8_t precharge_state = message->data[1];
        uint8_t ecu_state = message->data[2];
        uint8_t mock_value[8] = {04, 43, hv_state, precharge_state, ecu_state, 00, 00, 00};
        for(int i = 0; i < message->data_length_code; i++){
          printf("%x\t", mock_value[i]);
          //  // Append data to the file
           if (file.print(String(mock_value[i]))) {
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
        break;
      }

      case 0x18FE8D02:
      {
        File file = SD.open("/pdu_state", FILE_APPEND);
        if(!file){
          Serial.println("File does not exist yet");
        }
        else{
          Serial.println("File pdu state already exist");
        }
        uint8_t dcdc_state = message->data[0];
        uint8_t mock_value[8] = {01, 43, dcdc_state, 00, 00, 00, 00, 00};
        for(int i = 0; i < message->data_length_code; i++){
          printf("%x\t", mock_value[i]);
          //  // Append data to the file
           if (file.print(String(mock_value[i]))) {
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
        break;
      }
      case 0x18FE8D03:
      {
        File file = SD.open("/pdu_state", FILE_APPEND);
        if(!file){
          Serial.println("File does not exist");
        }
        else{
          Serial.println("File pdu state already exist");
        }
        uint8_t lv_state = message->data[0];
        uint8_t mock_value[8] = {01, 43, 00, lv_state, 00, 00, 00, 00};
        for(int i = 0; i < message->data_length_code; i++){
          printf("%x\t", mock_value[i]);
          //  // Append data to the file
           if (file.print(String(mock_value[i]))) {
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
        break;
      }
      case 0x18F11401:
      {
        File file = SD.open("/dcdc_state", FILE_APPEND);
        if(!file){
          Serial.println("File does not exist yet");
        }
        else{
          Serial.println("File dcdc state already exist");          
        }
        for(int i = 0; i < message->data_length_code; i++){
          printf("%x\t", message->data[i]);
          //  // Append data to the file
           if (file.print(String(message->data[i]))) {
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
        break;
      }                  
    }    
  }  
}

void receive_message(twai_message_t *message)
{
    if (twai_receive(message, pdMS_TO_TICKS(1000)) == ESP_OK) {
       File file = SD.open("/test_log.txt", FILE_APPEND);
       if (!file) {
        Serial.println("Failed to open file for appending");
      }
        // switch(message->identifier) {
        //     case 0x18FEF100:
        //         printf("\nVehicle Speed Physical Value: %d\n", message->data[2]);
        //         break;
        //     case 0xCF00400:
                // if (message->data_length_code >= 6) {
                //     // Extract MSB and LSB
                //     uint8_t msb = message->data[4];
                //     uint8_t lsb = message->data[3];

                //     // Combine MSB and LSB to form a 16-bit value
                //     uint16_t rpmValue = (msb << 8) | lsb;

                //     // Calculate the real RPM value
                //     float realRPM = rpmValue * 0.125;
                //     printf("Vehicle Engine RPM Physical Value: %.0f RPM\n", realRPM);
                //     printf("\nVehicle RPM in Hex:%x \t %x", msb, lsb);
                // }
        //         break;
        // }
        for(int i = 0;i < String(message->identifier).length();i++){
          if(file.print(String(message->identifier)[i])){
            Serial.println("ID Appended");
          }    
          else{
            Serial.println("Gagal append");
          }      
        }
        if(file.print("\t")){
          Serial.println("Tab Appended");
        }
        else{
          Serial.println("Gagal append");
        }
        for(int i = 0; i < message->data_length_code; i++){
          printf("%x\t", message->data[i]);
          //  // Append data to the file
           if (file.print(String(message->data[i]))) {
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

        switch(message->identifier){
          case 0x0CF00401:
            File file = SD.open("/inverter.txt", FILE_APPEND);
            if (!file) {
              Serial.println("Failed to open file for appending");
            }
            for(int i = 0; i < message->data_length_code; i++){
            printf("%x\t", message->data[i]);
            //  // Append data to the file
            if (file.print(String(message->data[i]))) {
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
        }

    } else {
        printf("Failed to receive message\n");
    }
    // logSDCard(message->data);
}


void receive_and_transmit_dtc_powertrain(twai_message_t *message){
  if (twai_receive(message, pdMS_TO_TICKS(1000)) == ESP_OK) {
        if(message->identifier == OBD2_REQUEST_ID){
          printf("Request Accepted/n");
          twai_message_t response;
          uint8_t dtcResponseData[3] = {0x50, 0x01, 0x23}; // Example DTC (P0123)
          new_message(&response, OBD2_RESPONSE_ID, 3, dtcResponseData);
          transmit_message(&response);          
        }
    } else {
        printf("Failed to receive message\n");
    }
}

void receive_and_transmit_dtc_body(twai_message_t *message){
  if (twai_receive(message, pdMS_TO_TICKS(1000)) == ESP_OK) {
        if(message->identifier == OBD2_REQUEST_ID){
          printf("Request Accepted/n");
          twai_message_t response;
          uint8_t dtcResponseData[3] = {0x42, 0x01, 0x23}; // Example DTC (B0123)
          new_message(&response, OBD2_RESPONSE_ID, 3, dtcResponseData);
          transmit_message(&response);          
        }
    } else {
        printf("Failed to receive message\n");
    }
}

void receive_and_transmit_dtc_chassis(twai_message_t *message){
  if (twai_receive(message, pdMS_TO_TICKS(1000)) == ESP_OK) {
        if(message->identifier == OBD2_REQUEST_ID){
          printf("Request Accepted/n");
          twai_message_t response;
          uint8_t dtcResponseData[3] = {0x43, 0x01, 0x23}; // Example DTC (C0123)
          new_message(&response, OBD2_RESPONSE_ID, 3, dtcResponseData);
          transmit_message(&response);          
        }
    } else {
        printf("Failed to receive message\n");
    }
}

void receive_and_transmit_dtc_network(twai_message_t *message){
  if (twai_receive(message, pdMS_TO_TICKS(1000)) == ESP_OK) {
        if(message->identifier == OBD2_REQUEST_ID){
          printf("Request Accepted/n");
          twai_message_t response;
          uint8_t dtcResponseData[3] = {0x55, 0x01, 0x23}; // Example DTC (U0123)
          new_message(&response, OBD2_RESPONSE_ID, 3, dtcResponseData);
          transmit_message(&response);          
        }
    } else {
        printf("Failed to receive message\n");
    }
}

void setup(){
  Serial.begin(115200);
  twai_setup_and_install_for_send();
  if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
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

  File file = SD.open("/test_log.txt");
  if(!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/test_log.txt", "test\r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  
  file.close();

  file  = SD.open("/dcdc_state.txt");
  if(!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/dcdc_state.txt", "test\r\n");
  }
  else {
    Serial.println("File dcdc state already exists");  
  }
  
  file.close();

  file = SD.open("/pdu_state", FILE_APPEND);
  if(!file){
    Serial.println("File does not exist yet");
    writeFile(SD, "/pdu_state.txt", "test\r\n");
  }
  else{
    Serial.println("File pdu state already exist");
  }
  file.close();

  file = SD.open("/bms_status", FILE_APPEND);
  if(!file){
    Serial.println("File does not exist yet");
    writeFile(SD, "/bms_status.txt", "test\r\n");
  }
  else{
    Serial.println("File bms state already exist");
  }
  file.close();

  file = SD.open("/inverter_state", FILE_APPEND);
  if(!file){
    Serial.println("File does not exist yet");
    writeFile(SD, "/inverter_state.txt", "test\r\n");
  }
  else{
    Serial.println("File inverter state already exist");
  }
  file.close();
}

void loop()
{
    twai_message_t message;
    twai_message_t message1;
    // Set the data to send
    uint8_t data[8] = {rand() % 255, rand() % 255, rand() % 255, 
    rand() % 255, rand() % 255, rand() % 255, rand() % 255, rand() % 255};

    int current = 0;

    // while(current <= 2000){ 
    //   current += 1;
    // // Create a new message
    // //new_message(&message, 0x123, 8, data);

    // // Transmit the message to a queue
    // //transmit_message(&message);

    // // Receive the message from the queue
    // // receive_message(&message1);

    // // // Wait for 1 second
    // // vTaskDelay(100 / portTICK_PERIOD_MS);
    // }
    if(millis() - prevMillis >= 2000){
      prevMillis = millis();
      // read_status(&message);
      read_inverter_status(&message);
      read_pdu_state1(&message);
      read_pdu_state2(&message);
      read_pdu_state3(&message);
      read_dcdc_state(&message);
      printf("\n Waktu: %d", millis());
      // receive_message(&message);
      current = 0;
    }

    // if(millis() - prevMillis >= 200){
    //   prevMillis = millis();
    //   read_inverter_status(&message);
    //   printf("\n Waktu1: %d", millis());
    //   current = 0;
    // }

    // if(millis() - prevMillis >= 400){
    //   prevMillis = millis();
    //   read_pdu_state1(&message);
    //   printf("\n Waktu2: %d", millis());
    //   current = 0;
    // }

    // if(millis() - prevMillis >= 600){
    //   prevMillis = millis();
    //   read_pdu_state2(&message);
    //   printf("\n Waktu3: %d", millis());
    //   current = 0;
    // }

    // if(millis() - prevMillis >= 800){
    //   prevMillis = millis();
    //   read_pdu_state3(&message);
    //   printf("\n Waktu4: %d", millis());
    //   current = 0;
    // }

    // if(millis() - prevMillis >= 1000){
    //   prevMillis = millis();
    //   read_dcdc_state(&message);
    //   printf("\n Waktu5: %d", millis());
    //   current = 0;
    // }

}