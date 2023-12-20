#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "BluetoothSerial.h"

// Check if Bluetooth configs are enabled
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Bluetooth Serial object
BluetoothSerial SerialBT;

String message = "";

char incomingChar;
int stop = 0;

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        SerialBT.write('Failed');
        delay(10);
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        // Serial.write(file.read());
        SerialBT.write(file.read());
        // delay(10);
    }
    file.close();
}

void readBluetooth(void *arg){
  while(1){
    if(SerialBT.available()){
      incomingChar = SerialBT.read();
      if (incomingChar != '\n'){
        message += String(incomingChar);
      }
      else{
        message = "";
      }
      // Serial.write(incomingChar);
    }
    if(message == "baca inverter"){
      readFile(SD, "/inverter_status.txt");
      printf("\nPanjang message: %d", message.length());
    }
    if(message == "baca pdu"){
      readFile(SD, "/pdu_status.txt");
      printf("\nPanjang message: %d", message.length());
    }
    if(message == "baca dcdc"){
      readFile(SD, "/dcdc_state.txt");
      printf("\nPanjang message: %d", message.length());
    }
    // Serial.printf("\nPesen: %s", message);
    // message = "";
  }
}

void setup() {
  // put your setup code here, to run once:
     Serial.begin(115200);
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

    SerialBT.begin("ESP32-Slave");
    Serial.println("The device started, now you can pair it with bluetooth!");

    // readFile(SD, "/dcdc_state.txt");
    xTaskCreate(readBluetooth, "read_bluetooth", 3000, NULL, 2, NULL);
    vTaskDelete(NULL); 
}

void loop() {
  // put your main code here, to run repeatedly:

}
