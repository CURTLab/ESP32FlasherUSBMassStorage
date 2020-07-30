#include "serial_io.h"

#include <Arduino.h>

static unsigned long s_time_end;
static int s_reset_trigger_pin;
static int s_gpio0_trigger_pin;

#include <USBComposite.h>
extern USBCompositeSerial CompositeSerial;

static void serial_debug_print(const uint8_t *data, uint16_t size, bool write) { }

HardwareSerial *s_serial;

esp_loader_error_t loader_port_serial_init(const loader_serial_config_t *config)
{
  s_reset_trigger_pin = (int)config->reset_trigger_pin;
  s_gpio0_trigger_pin = (int)config->gpio0_trigger_pin;

  if (config->uart_port == 1)
    s_serial = &Serial1;
  else if(config->uart_port == 2)
    s_serial = &Serial2;
  else if(config->uart_port == 3)
    s_serial = &Serial3;
  else
    return ESP_LOADER_ERROR_FAIL;
  
  pinMode(s_reset_trigger_pin, OUTPUT);
  pinMode(s_gpio0_trigger_pin, OUTPUT);
  digitalWrite(s_reset_trigger_pin, 1);
  digitalWrite(s_gpio0_trigger_pin, 1);
  
  s_serial->begin(config->baud_rate);
  //Serial1.begin(9600);
  return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_serial_write(const uint8_t *data, uint16_t size, uint32_t timeout)
{
  serial_debug_print(data, size, true);

  while (s_serial->available()) s_serial->read();
    
  unsigned long timer = millis();
  while(size) {
    if (s_serial->write(*data) != 1)
      return ESP_LOADER_ERROR_FAIL;
    ++data;
    --size;
    if (millis() - timer > timeout)
      return ESP_LOADER_ERROR_TIMEOUT;
  }
  return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_serial_read(uint8_t *data, uint16_t size, uint32_t timeout)
{
  serial_debug_print(data, size, false);

  uint8_t *buf = data;
  unsigned long timer = millis();
  
  while(size) {
    if (s_serial->available()) {
      *buf = s_serial->read();
      ++buf;
      --size;
    }
    if ((millis() - timer) > timeout)
      return ESP_LOADER_ERROR_TIMEOUT;
  }
  return (size != 0) ? ESP_LOADER_ERROR_FAIL : ESP_LOADER_SUCCESS;
}

// Set GPIO0 LOW, then
// assert reset pin for 50 milliseconds.
void loader_port_enter_bootloader(void)
{
  digitalWrite(s_gpio0_trigger_pin, 0);
  digitalWrite(s_reset_trigger_pin, 0);
  delay(150);
  digitalWrite(s_reset_trigger_pin, 1);
  delay(150);
  digitalWrite(s_gpio0_trigger_pin, 1);
}

void loader_port_reset_target(void)
{
  digitalWrite(s_gpio0_trigger_pin, 1);
  digitalWrite(s_reset_trigger_pin, 0);
  delay(150);
  digitalWrite(s_reset_trigger_pin, 1);
}

void loader_port_delay_ms(uint32_t ms)
{
  delay(ms);
}

void loader_port_start_timer(uint32_t ms)
{
  s_time_end = millis() + ms;
}

uint32_t loader_port_remaining_time(void)
{
  unsigned long remaining = (s_time_end - millis());
  return (remaining > 0) ? (uint32_t)remaining : 0;
}

void loader_port_debug_print(const char *str)
{
  CompositeSerial.print("\033[36m[D] ");
  CompositeSerial.print(str);
  CompositeSerial.println("\033[m");
}
