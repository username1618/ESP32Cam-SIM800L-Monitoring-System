#include "esp_camera.h"
#include "SPI.h"
#include "driver/rtc_io.h"
#include "soc/rtc_cntl_reg.h"
#include <FS.h>
#include "ftpParams.h"
//#include "gprsParams.h"
#include "mobile.h"
#include "Sim800lClient.h" // импортирует sim800lClient, deviceSettings и DEBUG_LOGS
#include "DHT.h"

// Конфигурационные константы
#define USER_ID "A"
#define SEND_MODE "HTTP" // HTTP or FTP
#define uS_TO_M_FACTOR 60000000ULL
#define PIR_SENSOR_ENABLED false
#define UART_WAKEUP_ENABLED false
#define LIGTH_SLEEP false
#define SKIP_SMALL_FILES false
#define MIN_FILE_SIZE_TO_SEND 1600
#define FLASH_LED_PIN 4
#define VOLT_SENSOR_PIN 12
#define DHT_PIN GPIO_NUM_13
#define RED_LED_PIN 33
#define GSM_RESET_PIN 2

// Конфигурация камеры
#define CAMERA_MODEL_AI_THINKER
#if defined(CAMERA_MODEL_AI_THINKER)
  #define PWDN_GPIO_NUM 32
  #define RESET_GPIO_NUM -1
  #define XCLK_GPIO_NUM 0
  #define SIOD_GPIO_NUM 26
  #define SIOC_GPIO_NUM 27
  #define Y9_GPIO_NUM 35
  #define Y8_GPIO_NUM 34
  #define Y7_GPIO_NUM 39
  #define Y6_GPIO_NUM 36
  #define Y5_GPIO_NUM 21
  #define Y4_GPIO_NUM 19
  #define Y3_GPIO_NUM 18
  #define Y2_GPIO_NUM 5
  #define VSYNC_GPIO_NUM 25
  #define HREF_GPIO_NUM 23
  #define PCLK_GPIO_NUM 22
#else
  #error "Camera model not selected"
#endif

// Глобальные переменные
int TIME_TO_SLEEP_MINUTES = 30;
int SIM800_START_DELAY_SEC = 60;
int MAX_RETRIES_TO_SEND_IMAGE = 3;

// Глобальные объекты
Sim800lClient sim800lClient;
ApiParams apiParams;
DHT dht(DHT_PIN, DHT11);



void setup() {
  // Инициализация аппаратных компонентов
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Отключаем детектор пониженного напряжения
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, HIGH); // Выключаем красный светодиод
  
  // Настройка GSM модуля
  pinMode(GSM_RESET_PIN, OUTPUT);
  digitalWrite(GSM_RESET_PIN, HIGH);
  rtc_gpio_hold_dis(GPIO_NUM_2);
  rtc_gpio_hold_dis(DHT_PIN);

  // Настройка пинов
  pinMode(DHT_PIN, INPUT);
  pinMode(VOLT_SENSOR_PIN, INPUT);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  // Инициализация последовательных портов
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 14, 15);

  // Загрузка настроек устройства из RTC памяти(заполняет объект deviceSettings)
  initDeviceSettings();

  // Обновляем глобальные переменные полученными настройками
  TIME_TO_SLEEP_MINUTES = deviceSettings.timeToSleepMinutes;
  SIM800_START_DELAY_SEC = deviceSettings.sim800StartDelaySec;
  MAX_RETRIES_TO_SEND_IMAGE = deviceSettings.maxRetriesToSendImage;


  // Инициализация камеры
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = getFrameSizeFromValue(deviceSettings.framesize);
  config.pixel_format = config.pixel_format = getPixelFormatFromValue(deviceSettings.pixelFormat);
  config.jpeg_quality = deviceSettings.quality;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    if (DEBUG_LOGS) {
      Serial.printf("Camera init failed with error 0x%x", err);
    }
    return;
  }

  // Настройка режимов сна
  if (DEBUG_LOGS) {
    Serial.printf("Setting up ESP32 to sleep every %d minutes\n", TIME_TO_SLEEP_MINUTES);
  }
  esp_sleep_enable_timer_wakeup(uS_TO_M_FACTOR * TIME_TO_SLEEP_MINUTES);
  
  if (PIR_SENSOR_ENABLED) {
    esp_sleep_enable_ext0_wakeup(DHT_PIN, 1);
  }  

  if (UART_WAKEUP_ENABLED) {
    gpio_wakeup_enable(GPIO_NUM_14, GPIO_INTR_HIGH_LEVEL);
    esp_sleep_enable_gpio_wakeup();
  }

  // Инициализация датчиков
  dht.begin();

  // Настройка камеры
  sensor_t* s = esp_camera_sensor_get();
  setupCamera(s);
}

void loop() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  printWakeupReason(wakeup_reason);

  // Обработка различных причин пробуждения
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    handleMotionDetection();
  }
  else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER || wakeup_reason == 0) {
    if (DEBUG_LOGS) {
      Serial.println("Starting normal operation...");
    }
    delay(SIM800_START_DELAY_SEC * 1000);
  }

  // Основная логика работы
  if (wakeup_reason != ESP_SLEEP_WAKEUP_GPIO) {
    executeMainLogic();
  }

  // Переход в режим сна
  prepareForSleep();
}

void handleMotionDetection() {
  if (DEBUG_LOGS) {
    Serial.println("Motion detected!");
  }
  delay(SIM800_START_DELAY_SEC * 1000);
  
  if (sim800lClient.waitForGsmNetwork()) {
    sim800lClient.sendSMS(phoneNumber, alarmSMSMessage);
    delay(10000);
    sim800lClient.makeCall(phoneNumber);
    sim800lClient.waitForCallToEnd();
  }
}

void executeMainLogic() {
  camera_fb_t *fb = takePhoto();
  if (!fb) {
    if (DEBUG_LOGS) {
      Serial.println("Camera capture failed");
    }
    return;
  }

  size_t fbLen = fb->len;
  if (DEBUG_LOGS) {
    Serial.println("Captured image size: " + String(fbLen));
  }

  if (SKIP_SMALL_FILES && fbLen < MIN_FILE_SIZE_TO_SEND) {
    if (DEBUG_LOGS) {
      Serial.println("Image too small, skipping send");
    }
    esp_camera_fb_return(fb);
    return;
  }

  bool photoSent = false;
  for (int attempt = 1; attempt <= MAX_RETRIES_TO_SEND_IMAGE && !photoSent; attempt++) {
    photoSent = sendPhoto(fb, SEND_MODE);
    if (!photoSent) {
      if (DEBUG_LOGS) {
        Serial.println("Error sending photo, attempt " + String(attempt));
      }
      sim800lClient.resetGsm(GSM_RESET_PIN);
      delay(SIM800_START_DELAY_SEC * 1000);
    }
  }

  esp_camera_fb_return(fb);
  sim800lClient.goToSleep();
}

void prepareForSleep() {
  if (DEBUG_LOGS) {
    Serial.println("Preparing for sleep: " + String(TIME_TO_SLEEP_MINUTES) + " minutes");
  }
  rtc_gpio_hold_en(GPIO_NUM_2);
  rtc_gpio_hold_en(DHT_PIN);
  Serial2.flush();
  
  if (LIGTH_SLEEP) {
    esp_light_sleep_start();
  } else {
    esp_deep_sleep_start();
  }
}


// Вспомогательные функции
pixformat_t getPixelFormatFromValue(int value) {
    switch(value) {
        case 0: return PIXFORMAT_JPEG;      // JPEG сжатие
        case 1: return PIXFORMAT_RGB565;    // 16 бит RGB
        case 2: return PIXFORMAT_YUV422;    // YUV422
        case 3: return PIXFORMAT_GRAYSCALE; // Ч/Б
        case 4: return PIXFORMAT_RAW;       // Сырые данные с сенсора
        default: return PIXFORMAT_JPEG;     // Значение по умолчанию
    }
}
framesize_t getFrameSizeFromValue(int value) {
    switch(value) {
        case 0: return FRAMESIZE_QVGA;
        case 1: return FRAMESIZE_CIF;
        case 2: return FRAMESIZE_VGA;
        case 3: return FRAMESIZE_SVGA;
        case 4: return FRAMESIZE_XGA;
        case 5: return FRAMESIZE_SXGA;
        case 6: return FRAMESIZE_UXGA;
        default: return FRAMESIZE_SXGA; // значение по умолчанию
    }
}

void setupCamera(sensor_t* s) {

    // 1. Яркость и Контрастность
    s->set_brightness(s, constrain(deviceSettings.brightness, -2, 2));
    s->set_contrast(s, constrain(deviceSettings.contrast, -2, 2));

    // 2. Экспозиция (OV2640 чувствительна к этим настройкам)
    s->set_exposure_ctrl(s, deviceSettings.aec);
    if(!deviceSettings.aec) {
        s->set_aec_value(s, constrain(deviceSettings.aecValue, 0, 1200));

        // AEC2 (DSP-экспозиция) имеет смысл только при включенном AEC
        // Для AI Thinker лучше отключать при высоких разрешениях
        s->set_aec2(s, deviceSettings.aec2 && (deviceSettings.framesize <= FRAMESIZE_XGA));
    }
    s->set_ae_level(s, constrain(deviceSettings.aeLevel, -2, 2));


    // 3. Усиление (для AI Thinker лучше ограничить ceiling)
    s->set_gain_ctrl(s, deviceSettings.agc);
    if(!deviceSettings.agc) {
        s->set_agc_gain(s, constrain(deviceSettings.agcGain, 0, 30));
    }
    s->set_gainceiling(s, (gainceiling_t)constrain(deviceSettings.gainCeiling, 0, 4)); // Max 32x для стабильности


    // 4. Баланс белого
    s->set_whitebal(s, deviceSettings.awb);
    if(!deviceSettings.awb) {
        s->set_wb_mode(s, deviceSettings.wbMode);
    }

    // 5. Спецэффекты, насыщенность и ориентация
    if(deviceSettings.specialEffect != 0) {
        s->set_whitebal(s, false);  // Отключаем AWB для спецэффектов
        s->set_awb_gain(s, false);  // Отключаем усиление AWB
        if(deviceSettings.specialEffect == 1 ||  // Negative
          deviceSettings.specialEffect == 2 ||  // Grayscale
          deviceSettings.specialEffect == 6) {  // Sepia
            s->set_saturation(s, -2); // Максимальное обесцвечивание
        }
    } else {
      // Устанавливаем насыщенность, если спецэффектов нет
        s->set_saturation(s, deviceSettings.saturation);
    }
    s->set_hmirror(s, deviceSettings.hmirror);
    s->set_vflip(s, deviceSettings.vflip);


    // 6. Коррекция изображения
    s->set_bpc(s, deviceSettings.bpc); 
    s->set_wpc(s, deviceSettings.wpc); 
    s->set_raw_gma(s, deviceSettings.rawGma);
    s->set_lenc(s, deviceSettings.lensCorrection); // Рекомендуется всегда отключать - аппаратный дефект платы
    s->set_dcw(s, deviceSettings.dcw); // Рекомендуется вкл. для разрешений >SVGA
    if(deviceSettings.pixelFormat == PIXFORMAT_JPEG) {
      s->set_quality(s, constrain(deviceSettings.quality, 10, 63));
    }


    // 7. Особые фиксы для этой платы
    if(deviceSettings.bankSelFix) {
        s->set_reg(s, 0xFF, 0x01, 0x01); // BANK SEL
    }
    if(deviceSettings.clockSpeedFix) {    
        s->set_reg(s, 0x11, 0x01, 0x01); // Clock speed adjustment
    }
    if(deviceSettings.hrefFix) {
        s->set_reg(s, 0x15, 0x00, 0x00); // HREF не обрезать
    }
    if(deviceSettings.jpegQualityFix) {
        s->set_reg(s, 0x44, 0x06, 0x06); // Фикс артефактов JPEG
    }
    if(deviceSettings.pinkTintFix) {    // Фикс розового оттенка:
        s->set_reg(s, 0xFF, 0x00, 0x00);
        s->set_reg(s, 0x7C, 0x00, 0x00);
        s->set_reg(s, 0x7D, 0x02, 0x02);
        s->set_reg(s, 0x7C, 0x03, 0x03);
        s->set_reg(s, 0x7D, 0x48, 0x48);
    }

    delay(100); // Даем камере время на перестройку
    if(DEBUG_LOGS) Serial.println("Camera settings applied (fully optimized)");
}


float getVoltage() {
    int numReadings = deviceSettings.numReadingsVoltage;               // Количество измерений
    int totalDurationSec = deviceSettings.totalDurationSecVoltage;     // Общая продолжительность замеров (сек)
    const int minDelayMs = 50;  // Минимальная задержка между замерами
    
    // Рассчитываем интервал между замерами
    const int actualDelay = max((totalDurationSec * 1000) / numReadings, minDelayMs);
    float readings[numReadings]; // Статический массив для хранения значений
    
    // Цикл измерений
    for (int i = 0; i < numReadings; i++) {
        int adcValue = analogRead(VOLT_SENSOR_PIN);
        adcValue = round(adcValue / 50.0f) * 50.0f;  // Квантование значений
        readings[i] = 0.005f * adcValue - 1.0f;      // Преобразование в вольты
        
        if (i < numReadings - 1) delay(actualDelay); // Задержка между замерами
    }

    // Сортировка
    sortArray(readings, numReadings);

    // Расчет медианы
    float median = (numReadings % 2) ? 
        readings[numReadings / 2] : 
        (readings[numReadings / 2 - 1] + readings[numReadings / 2]) * 0.5f;

    return round(median * 10.0f) / 10.0f;    // Округление до 0.1 Вольт
}

std::pair<float, float> readDHT() {
    const int maxAttempts = 10; // Максимальное количество попыток получить валидные данные
    int numReadings = deviceSettings.numReadingsDHT; // Количество замеров для медианного фильтра
    int totalDurationSec = deviceSettings.totalDurationSecDHT; // Общая продолжительность замеров (сек)
    const int minDelayMs = 100; // Минимальная задержка между замерами
    
    // Рассчитываем интервал между замерами
    const int actualDelay = max((totalDurationSec * 1000) / numReadings, minDelayMs);

    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        float h = dht.readHumidity();
        float t = dht.readTemperature();

        if (!isnan(h) && !isnan(t)) {
            float hReadings[numReadings], tReadings[numReadings];
            
            // Первые значения - уже полученные
            hReadings[0] = h;
            tReadings[0] = t;

            // Дополнительные замеры с рассчитанной задержкой
            for (int i = 1; i < numReadings; ++i) {
                delay(actualDelay); // Задержка между измирениями
                hReadings[i] = dht.readHumidity();
                tReadings[i] = dht.readTemperature();
            }

            // Сортировка и расчет медианы
            sortArray(hReadings, numReadings);
            sortArray(tReadings, numReadings);

            float medianH = numReadings % 2 ? 
                hReadings[numReadings/2] : 
                (hReadings[numReadings/2 - 1] + hReadings[numReadings/2]) * 0.5f;

            float medianT = numReadings % 2 ? 
                tReadings[numReadings/2] : 
                (tReadings[numReadings/2 - 1] + tReadings[numReadings/2]) * 0.5f;

            return {medianH, medianT};
        }
        delay(1000);  // Задержка между попытками
    }

    return {NAN, NAN};  // Возвращаем ошибку
}

void sortArray(float* arr, int size) {
  for (int i = 0; i < size-1; i++) {
    for (int j = 0; j < size-i-1; j++) {
      if (arr[j] > arr[j+1]) {
        float temp = arr[j];
        arr[j] = arr[j+1];
        arr[j+1] = temp;
      }
    }
  }
}

void printWakeupReason(esp_sleep_wakeup_cause_t wakeup_reason) {
  const char* reasons[] = {
    "External signal using RTC_IO",
    "External signal using RTC_CNTL",
    "Timer",
    "Touchpad",
    "ULP program",
    "GPIO",
    "UART",
    "Unknown"
  };
  
  int index = wakeup_reason < 7 ? wakeup_reason : 7;
  if (DEBUG_LOGS) {
    Serial.printf("Wakeup caused by: %s\n", reasons[index]);
  }
}

void setFlash(bool on) {
  digitalWrite(FLASH_LED_PIN, on);
}

void blinkFlash(int n, int onTime, int offTime) {
  for (int i = 0; i < n; i++) {
    setFlash(true);
    delay(onTime);
    setFlash(false);
    if (i < n - 1) delay(offTime);
  }
}

// Получает и сохраняет новые настройки с сервера
bool fetchAndStoreSettings() {
    DeviceSettings newSettings;
    
    // 1. Получаем настройки с сервера
    if (!sim800lClient.getCameraSettingsHttp(&apiParams, &newSettings)) {
        if (DEBUG_LOGS) {
          Serial.println("Error: Failed to get settings from server");
        }
        blinkFlash(4, 1000, 500); // Визуальное оповещение об ошибке
        sim800lClient.stopHttp();
        return false;
    }

    // 2. Сохраняем в RTC память
    if (!saveSettingsToRTC(newSettings)) {
        if (DEBUG_LOGS) {
          Serial.println("Error: Failed to save to RTC");
        }
        return false;
    }

    // 3. Обновляем глобальный кэш
    deviceSettings = newSettings;

    // 4. Оперативно меняем параметр, чтобы esp32 ушла в сон с новым значением
    TIME_TO_SLEEP_MINUTES = deviceSettings.timeToSleepMinutes;
    esp_sleep_enable_timer_wakeup(uS_TO_M_FACTOR * TIME_TO_SLEEP_MINUTES);

    if (DEBUG_LOGS) {
      Serial.println("Settings updated successfully:");
      Serial.printf("Quality: %d, Sleep: %d min\n", 
                  deviceSettings.quality, deviceSettings.timeToSleepMinutes);
    }

    return true;
}


void getAndSendSms() {
    if (!deviceSettings.returnSms) return;
    
    if (DEBUG_LOGS) {
        Serial.println("Проверка новых СМС...");
    }
    
    // Получаем новые СМС
    std::vector<SmsMessage> newSms = sim800lClient.getNewSms();
    
    if (!newSms.empty() && DEBUG_LOGS) {
        Serial.print("Найдено новых СМС: ");
        Serial.println(newSms.size());
    }
    
    // Отправляем их на сервер пачками по N шт (в соответствии с настройками)
    int batchSms = deviceSettings.batchSms;
    
    for (size_t i = 0; i < newSms.size(); i += batchSms) {
        size_t end = (i + batchSms < newSms.size()) ? i + batchSms : newSms.size();
        std::vector<SmsMessage> batch(newSms.begin() + i, newSms.begin() + end);
        
        if (DEBUG_LOGS) {
            Serial.println("Отправка пачки СМС: " + String(i/batchSms + 1) + 
                          " из " + String((newSms.size() + batchSms - 1) / batchSms));
        }
        
        if (!sim800lClient.sendSmsToServer(batch, &apiParams)) {
            if (DEBUG_LOGS) {
                Serial.println("Ошибка отправки пачки СМС");
            }
            // Добавляем задержку перед повторной попыткой
            delay(1000);
        } else {
            // Добавляем задержку между пачками для стабильности
            delay(500);
        }
    }
}



camera_fb_t* takePhoto() {
  const int flashDelay = deviceSettings.flashDelay;
  const int photoCount = deviceSettings.photoCount;
  const int photoDelay = deviceSettings.photoDelay;
  
  // Включаем вспышку
  setFlash(true);
  delay(flashDelay);

  // Делаем несколько кадров для срабатывания автонастройки камеры
  for (int i = 0; i < photoCount - 1; i++) {
    camera_fb_t* tempFb = esp_camera_fb_get();
    esp_camera_fb_return(tempFb);
    delay(photoDelay);
  }

  // Основной кадр
  camera_fb_t* fb = esp_camera_fb_get();

  // Выключаем вспышку
  setFlash(false);
  
  return fb;
}


bool sendPhoto(camera_fb_t* fb, const String& mode) {
  if (!sim800lClient.waitForGsmNetwork()) {
    if (DEBUG_LOGS) {
      Serial.println("No GSM network");
    }
    blinkFlash(2, 2000, 500);
    return false;
  }

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  // Получаем данные от модуля SIM800
  String simVoltage = sim800lClient.getBatteryVoltage();
  String simSignal = sim800lClient.getSignalStrength();

  // Получаем данные с датчиков
  auto [humidity, temperature] = readDHT();
  float akbVoltage = getVoltage();

  // Формируем имя файла
  String imageFileName = String(USER_ID) +
                       "_volt_" + simVoltage +
                       "_sig_" + simSignal +
                       "_up_" + String(wakeup_reason) +
                       "_t_" + String(temperature) +
                       "_h_" + String(humidity) +
                       "_akb_" + String(akbVoltage) +
                       ".jpg";

  bool result = false;
  if (mode == "HTTP") {
    // Получение настроек APN для текущего оператора
    String networkOperator = sim800lClient.getNetworkOperator();
    APNSettings apnSettings = APNSettings::getAPNSettings(networkOperator);
    
    if (sim800lClient.startHttp(&apiParams, &apnSettings)) {
        // Отправляем кадр на сервер
        result = sim800lClient.sendFileByHttp(fb, imageFileName, &apiParams);

        // Пытаемся получить от сервера новые настройки 
        fetchAndStoreSettings(); 

        // Считываем новые СМС и отправляем их на сервер (если нужно)
        getAndSendSms();

        sim800lClient.stopHttp();
    }

  } 
  else if (mode == "FTP") {
    if (sim800lClient.initFtp(ftpServerAddress, ftpServerPort, ftpUser, ftpPassword)) {
      result = sim800lClient.sendFileToFtp(fb, imageFileName);
      sim800lClient.stopFtp();
    }
  }
  else {
    if (DEBUG_LOGS) {
      Serial.println("Invalid SEND_MODE");
    }
  }

  if (!result) {
    if (DEBUG_LOGS) {
      Serial.println("Error sending file by " + mode);
    }
    blinkFlash(4, 1000, 500);
  }
  
  return result;
}