#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdio.h"
#include "stdlib.h"
#include "driver/twai.h"
#include "SD.h"
#include "SPI.h"
#include "FS.h"

uint8_t potValue[8] = {04, 41, 12, 00, 00, 00, 00, 00};

#define RX_PIN 21
#define TX_PIN 22

void twai_setup_and_install(){
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
    
    message->flags = TWAI_MSG_FLAG_EXTD;
    message->identifier = id;
    message->data_length_code = dlc;
    for (int i = 0; i < dlc; i++) {
        message->data[i] = data[i];
    }
    printf("Message created\nID: %x DLC: %d Data:\t", message->identifier, message->data_length_code);
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

void receive_message(twai_message_t *message)
{
    if (twai_receive(message, pdMS_TO_TICKS(1000)) == ESP_OK) {
        //printf("Message received:\n");
        //char ID = message->identifier;
        //printf(ID);
        // switch(message->identifier){
        //   case 14FEF121:
        //     printf("Glitched");
        //     break;          
        // }
        // twai_message_t message;
        // twai_message_t message1;
        // if(message->extd){
        //   new_message(&message1, 0x18DB33F1, 8, potValue);
        //   transmit_message(&message1);
        //   Serial.println("Message Extended successfully sent");          
        // }
        // else{
        //   new_message(&message1, 0x7DF, 8, potValue);
        //   transmit_message(&message1);
        //   Serial.println("Standard message successfully sent");
        // }
        printf("Masuk\n");
        printf("ID: %x DLC: %d Data: \t", message->identifier, message->data_length_code);
        for (int i = 0; i < message->data_length_code; i++) {
            // (message->extd)?printf("Extended ID"):printf("Standard ID");
            printf("%02x\t", message->data[i]);
        }
        
        // uint8_t msb = message->data[3];
        // uint8_t lsb = message->data[2];
        // uint16_t rpmValue = (msb << 8) | lsb;

        // // Calculate the real RPM value
        // float realRPM = rpmValue;
        // printf("Potentio real Value: %.0f RPM\n", realRPM);
        // printf("\nPotentio value in Hex: %x \t %x", msb, lsb);
        
    } else {
        printf("Failed to receive message\n");
    }
}

// variable for storing the potentiometer value
int potenValue = 0;
const int potPin = 4;

void setup() {
  Serial.begin(115200);
  twai_setup_and_install();
  delay(1000);
}

void loop() {
  // Reading potentiometer value
  twai_message_t message;
  twai_message_t message1;
  potenValue = analogRead(potPin);

  // Extract the most significant and least significant bytes
  uint8_t msb = (potenValue >> 8) & 0xFF;
  uint8_t lsb = potenValue & 0xFF;

  // Store the values in your array
  potValue[4] = msb;
  potValue[3] = lsb;

  // printf("\n");
  // for (int i = 0; i < 8; i++) {
  //   printf("%02x\t", potValue[i]);
  // }
  // printf("Potentiometer int Val: \t%d", potenValue); 
  // printf("\tPotentiometer MSB: \t%02x", msb); 
  // printf("\tPotentiometer LSB: \t%02x", lsb); 

  new_message(&message, 0x18DB33F1, 8, potValue);

  // Transmit the message to a queue
  transmit_message(&message);

  // Receive the message from the queue
  // receive_message(&message);

  // Wait for 1 second
  vTaskDelay(100 / portTICK_PERIOD_MS);
}
