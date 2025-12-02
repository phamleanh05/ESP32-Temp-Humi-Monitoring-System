#include "global.h"

#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"
// #include "mainserver.h"
#include "tinyml.h"
#include "coreiot.h"
#include "wifi_config.h"

// include task
#include "task_check_info.h"
#include "task_toogle_boot.h"
#include "task_wifi.h"
#include "task_core_iot.h"


void setup()
{
  Serial.begin(115200);
  check_info_File(0);

  // xTaskCreate(led_blinky, "Task LED Blink", 2048, NULL, 2, NULL);
  // xTaskCreate(neo_blinky, "Task NEO Blink", 2048, NULL, 2, NULL);
  xTaskCreate(temp_humi_monitor, "Task TEMP HUMI Monitor", 2048, NULL, 2, NULL);
  // Need turn of led_blynk and neo_blynk function
  xTaskCreate(wifi_config_task, "WiFi Config Task", 8192, NULL, 3, NULL);
  // xTaskCreate(main_server_task, "Task Main Server" ,8192  ,NULL  ,2 , NULL);
  // xTaskCreate(tiny_ml_task, "Tiny ML Task" ,2048  ,NULL  ,2 , NULL);
  // xTaskCreate(coreiot_task, "CoreIOT Task" ,4096  ,NULL  ,2 , NULL);
  // xTaskCreate(Task_Toogle_BOOT, "Task_Toogle_BOOT", 4096, NULL, 2, NULL);
}

void loop()
{
  // WiFi config handles its own connection management in FreeRTOS task
  if (check_info_File(1))
  {
    // Check if WiFi config has established connection
    if (wifiConfig && wifiConfig->isConnected())
    {
      // WiFi is connected via config system
      //CORE_IOT_reconnect();
    }
    else
    {
      // Let WiFi config handle all connection management
      Serial.println("WiFi config system will handle connection");
      //CORE_IOT_reconnect();
    }
  }
  
  delay(1000);
}