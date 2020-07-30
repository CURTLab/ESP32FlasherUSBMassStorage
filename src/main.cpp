#include <Arduino.h>
#include <USBComposite.h>
#include "fs/vFat16.h"
#include "esp_loader/esp_loader.h"
#include "esp_loader/serial_io.h"

#define PRODUCT_ID 0x29
const uint32_t APP_START_ADDRESS = 0x10000;

USBMassStorage MassStorage;
USBCompositeSerial CompositeSerial;

vFat16 vFat;

#define ESP32_BOOT PA1
#define ESP32_EN PB0

bool writeFat(const uint8_t *writebuff, uint32_t memoryOffset, uint16_t transferLength) {
  return vFat.write(writebuff, memoryOffset, transferLength);
}

bool readFat(uint8_t *readbuff, uint32_t block_no, uint16_t TotalBlocks) {
  return vFat.read(readbuff, block_no, TotalBlocks);
}

bool outputActive = false;
bool updatedStated = false;
bool updateError = false;
size_t image_size = 0;
int32_t packet_number = 0;
int32_t total_packages = 0;

#define HIGHER_BAUD_RATE 230400

bool bin_update_cb(const uint8_t *data, uint32_t block_no, uint32_t fileSize)
{
  esp_loader_error_t err;
  if (!updatedStated && !updateError) {
    // https://github.com/espressif/esptool/wiki/Firmware-Image-Format
    uint8_t hdr[3];
    memcpy(hdr, data, 3);
    if ((hdr[0] != 0xE9) || // magic number
        ((hdr[1] & 0xf0) != 0) ||
        (hdr[1] == 0) ||
        ((hdr[2] & 0x03) == 0) // SPI Flash Interface (0-3)
       ) 
    {
      updateError = true;
      CompositeSerial.print("\033[31m[E] Magic number wrong at block");
      CompositeSerial.print(block_no);
      CompositeSerial.println("!\033[m");
      return false;
    }

    Serial2.flush();

    loader_serial_config_t config;
    config.reset_trigger_pin = ESP32_EN; // EN
    config.gpio0_trigger_pin = ESP32_BOOT; // BOOT
    config.uart_port = 2;           // Serial2 (RX = PA3, TX = PA2)
    //config.baud_rate = 115200;
    config.baud_rate = 230400; // works, not working: 460800 or 921600
    err = loader_port_serial_init(&config);
    if (err != ESP_LOADER_SUCCESS)
    {
      CompositeSerial.println("\033[31m[E] Serial initialization failed.\033[m");
      return false;
    }

    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();
    err = esp_loader_connect(&connect_config);
    if (err != ESP_LOADER_SUCCESS)
    {
      CompositeSerial.print("\033[31m[E");
      CompositeSerial.print(err);
      CompositeSerial.println("] Cannot connect to target.\033[m");
      loader_port_reset_target();
      return false;
    }

    err = esp_loader_flash_start(APP_START_ADDRESS, image_size + APP_START_ADDRESS, 512);
    if (err != ESP_LOADER_SUCCESS) {
      CompositeSerial.print("\033[31m[E");
      CompositeSerial.print(err);
      CompositeSerial.println("] Flash start operation failed.\033[m");
      loader_port_reset_target();
      return false;
    }

#if 0
    err = esp_loader_change_baudrate(HIGHER_BAUD_RATE);
    if (err != ESP_LOADER_SUCCESS) {
        CompositeSerial.print("\033[33m[W] Unable to change baud rate on target.\033[m");
    }

    err = loader_port_change_baudrate(HIGHER_BAUD_RATE);
    if (err != ESP_LOADER_SUCCESS) {
        CompositeSerial.print("\033[33m[W] Unable to change baud rate on target.\033[m");
    }
#endif

    CompositeSerial.println("\033[32m[I] Start programming! \033[m");
    
    image_size = fileSize;
    packet_number = 0;
    total_packages = (image_size + 511) / 512;

    updatedStated = true;
  }

  if (updatedStated && (image_size > 0) && !updateError) {
    const size_t bytes = image_size < 512 ? image_size : 512;
    image_size -= bytes;
    ++packet_number;
    err = esp_loader_flash_write(const_cast<uint8_t*>(data), bytes);
    if (err != ESP_LOADER_SUCCESS)
    {
      CompositeSerial.print("\033[31m[E");
      CompositeSerial.print(err);
      CompositeSerial.print("] Packet ");
      CompositeSerial.print(packet_number);
      CompositeSerial.println(" could not be written!\033[m");
      updatedStated = false;
      updateError = true;
      return false;
    }

    CompositeSerial.print("\033[32m[I] Packet: ");
    CompositeSerial.print(packet_number);
    CompositeSerial.print("/");
    CompositeSerial.print(total_packages);
    CompositeSerial.print(", written: ");
    CompositeSerial.print(bytes);
    CompositeSerial.println(" B\033[m");

    if (image_size <= 0) {
      err = esp_loader_flash_verify();
      if (err != ESP_LOADER_SUCCESS) {
        CompositeSerial.print("\033[33m[W");
        CompositeSerial.print(err);
        CompositeSerial.println("] MD5 does not match!\033[m");
      } else {
        CompositeSerial.println("\033[32m[I] Flash verified! Restart target. \033[m");
      }
      loader_port_reset_target();
      pinMode(ESP32_EN, INPUT);
      pinMode(ESP32_BOOT, INPUT);
      Serial2.begin(115200);

      updateError = false;
      updatedStated = false;
      outputActive = true;
    }
  }
  if (updateError) {
    image_size -= 512;
    ++packet_number;
    if (packet_number == total_packages) {
      outputActive = true;
      updatedStated = false;
      updateError = false;
      image_size = 0;
      packet_number = 0;
      total_packages = 0;
      Serial2.begin(115200);
      CompositeSerial.println("\033[32m[I] Cleared error!\033[m");
      loader_port_reset_target();
    }
  }
  return true; 
}

void setup() {
  USBComposite.setProductId(PRODUCT_ID);
  
  CompositeSerial.registerComponent();
  
  vFat.begin(4 * 1024 * 1024, "GeigerBoot");

  MassStorage.setDriveData(0, vFat.vmemory()/SCSI_BLOCK_SIZE, readFat, writeFat);
  MassStorage.registerComponent();
  USBComposite.begin();

  vFat.addTextFile("DETAILS", "Copy ESP32 binary for flashing");  
  vFat.addFileWriteCallback("BIN", bin_update_cb);
  
  CompositeSerial.begin();

  if (outputActive)
    Serial2.begin(115200);
}

void loop() {
  MassStorage.loop();

  if (!updatedStated && outputActive) {
    // esp32 output
    if (Serial2.available()) {
      // color in green
      // https://community.platformio.org/t/monitor-configure-colors-for-line-with-tags/8625
      CompositeSerial.print("\033[32m");
      while (Serial2.available())
        CompositeSerial.write(Serial2.read());
      CompositeSerial.print("\033[m");
    }
  }
}