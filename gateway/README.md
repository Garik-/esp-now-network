я пытаюсь использовать WIFI_MODE_APSTA, для esp-now важно определить фиксированный channel 

1. esp_wifi_start
2. esp_wifi_set_channel
3. esp_now_init

^ это работает для WIFI_MODE_AP

но я хочу подключиться к другой точке доступа STA и когда я к ней подключаюсь происходит перераспределение канала
```
I (1808) wifi:ap channel adjust o:6,0 n:7,0
```

https://github.com/nopnop2002/esp-idf-espnow-gateway

WIFI_IF_AP == ESP_IF_WIFI_AP

