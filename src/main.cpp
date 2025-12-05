#include "global.h"

#include "webserver_wifi_config.h"

// include task
#include "task_read_dht11.h"
#include "task_check_info.h"
#include "task_toogle_boot.h"
#include "task_wifi.h"
#include "task_light_sensor.h"
#include "task_lcd.h"


void setup()
{
  Serial.begin(115200);
  check_info_File(0);

  xTaskCreate(temp_humi_monitor, "Task TEMP HUMI Monitor", 2048, NULL, 2, NULL);
  xTaskCreate(task_light_sensor, "Task Light Sensor", 2048, NULL, 2, NULL);
  xTaskCreate(task_lcd, "Task LCD Display", 2048, NULL, 1, NULL);
  // Need turn of led_blynk and neo_blynk function
  xTaskCreate(webserver_wifi_config_task, "WebServer WiFi Config Task", 8192, NULL, 3, NULL);
  // xTaskCreate(Task_Toogle_BOOT, "Task_Toogle_BOOT", 4096, NULL, 2, NULL);
}

void loop()
{
  if (check_info_File(1))
  {
    // Check if WiFi config has established connection
    if (wifiConfig && wifiConfig->isConnected())
    {

    }
    else
    {
      Serial.println("WiFi config system will handle connection");
      //CORE_IOT_reconnect();
    }
  }
  
  delay(1000);
}