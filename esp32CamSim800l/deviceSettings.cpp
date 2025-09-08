#include <Arduino.h>
#include "deviceSettings.h"

// Основной кэш с настройками устройства
DeviceSettings deviceSettings;

// Объявляем полную копию настроек в RTC памяти
RTC_DATA_ATTR DeviceSettings rtcDeviceSettings;
RTC_DATA_ATTR bool rtcInitialized = false;


/// Функции для парсинга настроек Камеры ///

String extractJson(String response) {
    int jsonStart = response.indexOf('{');
    int jsonEnd = response.lastIndexOf('}');
    if (jsonStart == -1 || jsonEnd == -1) return "";
    return response.substring(jsonStart, jsonEnd + 1);
}

bool parseCameraSettings(String json, DeviceSettings* settings) {
    // Проверяем наличие ключа и обновляем только если он есть

    // "Device" settings
    updateIntIfExists(json, "\"Device_General_timeToSleepMinutes\":", &settings->timeToSleepMinutes);
    updateIntIfExists(json, "\"Device_PhotoCapture_flashDelay\":", &settings->flashDelay);
    updateIntIfExists(json, "\"Device_PhotoCapture_photoCount\":", &settings->photoCount);
    updateIntIfExists(json, "\"Device_PhotoCapture_photoDelay\":", &settings->photoDelay);
    updateIntIfExists(json, "\"Device_Network_sim800StartDelaySec\":", &settings->sim800StartDelaySec);
    updateIntIfExists(json, "\"Device_Network_sim800TransportDelay\":", &settings->sim800TransportDelay);
    updateIntIfExists(json, "\"Device_Network_sim800PostDelay\":", &settings->sim800PostDelay);
    updateIntIfExists(json, "\"Device_Network_maxRetriesToSendImage\":", &settings->maxRetriesToSendImage);
    updateBoolIfExists(json, "\"Device_SendSMS_returnSms\":", &settings->returnSms);
    updateBoolIfExists(json, "\"Device_SendSMS_deleteSmsAfterSend\":", &settings->deleteSmsAfterSend);
    updateStringIfExists(json, "\"Device_SendSMS_smsReadMode\":", &settings->smsReadMode);
    updateIntIfExists(json, "\"Device_SendSMS_batchSms\":", &settings->batchSms);

    // "DHT" settings
    updateIntIfExists(json, "\"Sensors_DHT_numReadings\":", &settings->numReadingsDHT);
    updateIntIfExists(json, "\"Sensors_DHT_totalDurationSec\":", &settings->totalDurationSecDHT);

    // "Voltage" settings
    updateIntIfExists(json, "\"Sensors_Voltage_numReadings\":", &settings->numReadingsVoltage);
    updateIntIfExists(json, "\"Sensors_Voltage_totalDurationSec\":", &settings->totalDurationSecVoltage);

    // "Camera" settings
    updateIntIfExists(json, "\"Camera_Capture_framesize\":", &settings->framesize);
    updateIntIfExists(json, "\"Camera_Capture_pixelFormat\":", &settings->pixelFormat);
    updateIntIfExists(json, "\"Camera_Capture_quality\":", &settings->quality);
    updateIntIfExists(json, "\"Camera_Exposure_aeLevel\":", &settings->aeLevel);
    updateBoolIfExists(json, "\"Camera_Exposure_aec\":", &settings->aec);
    updateBoolIfExists(json, "\"Camera_Exposure_aec2\":", &settings->aec2);
    updateIntIfExists(json, "\"Camera_Exposure_aecValue\":", &settings->aecValue);
    updateBoolIfExists(json, "\"Camera_Gain_agc\":", &settings->agc);
    updateIntIfExists(json, "\"Camera_Gain_agcGain\":", &settings->agcGain);
    updateIntIfExists(json, "\"Camera_Gain_gainCeiling\":", &settings->gainCeiling);
    updateBoolIfExists(json, "\"Camera_HardwareFixes_bankSelFix\":", &settings->bankSelFix);
    updateBoolIfExists(json, "\"Camera_HardwareFixes_clockSpeedFix\":", &settings->clockSpeedFix);
    updateBoolIfExists(json, "\"Camera_HardwareFixes_hrefFix\":", &settings->hrefFix);
    updateBoolIfExists(json, "\"Camera_HardwareFixes_jpegQualityFix\":", &settings->jpegQualityFix);
    updateBoolIfExists(json, "\"Camera_HardwareFixes_pinkTintFix\":", &settings->pinkTintFix);
    updateIntIfExists(json, "\"Camera_ImageAdjustment_brightness\":", &settings->brightness);
    updateIntIfExists(json, "\"Camera_ImageAdjustment_contrast\":", &settings->contrast);
    updateBoolIfExists(json, "\"Camera_ImageAdjustment_hmirror\":", &settings->hmirror);
    updateIntIfExists(json, "\"Camera_ImageAdjustment_saturation\":", &settings->saturation);
    updateIntIfExists(json, "\"Camera_ImageAdjustment_specialEffect\":", &settings->specialEffect);
    updateBoolIfExists(json, "\"Camera_ImageAdjustment_vflip\":", &settings->vflip);
    updateBoolIfExists(json, "\"Camera_ImageProcessing_bpc\":", &settings->bpc);
    updateBoolIfExists(json, "\"Camera_ImageProcessing_dcw\":", &settings->dcw);
    updateBoolIfExists(json, "\"Camera_ImageProcessing_lensCorrection\":", &settings->lensCorrection);
    updateBoolIfExists(json, "\"Camera_ImageProcessing_rawGma\":", &settings->rawGma);
    updateBoolIfExists(json, "\"Camera_ImageProcessing_wpc\":", &settings->wpc);
    updateBoolIfExists(json, "\"Camera_WhiteBalance_awb\":", &settings->awb);
    updateBoolIfExists(json, "\"Camera_WhiteBalance_awbGain\":", &settings->awbGain);
    updateIntIfExists(json, "\"Camera_WhiteBalance_wbMode\":", &settings->wbMode);

    return true;
}
void updateBoolIfExists(const String& json, const String& key, bool* value) {
    int pos = json.indexOf(key);
    if (pos != -1) {
        int valueStart = pos + key.length();
        String valStr = json.substring(valueStart, valueStart + 5);
        *value = valStr.indexOf("true") != -1;
    }
}
void updateIntIfExists(const String& json, const String& key, int* value) {
    int pos = json.indexOf(key);
    if (pos != -1) {
        int valueStart = pos + key.length();
        int valueEnd = json.indexOf(',', valueStart);
        if (valueEnd == -1) valueEnd = json.indexOf('}', valueStart);
        *value = json.substring(valueStart, valueEnd).toInt();
    }
}
void updateFloatIfExists(const String& json, const String& key, float* value) {
    int pos = json.indexOf(key);
    if (pos != -1) {
        int valueStart = pos + key.length();
        int valueEnd = json.indexOf(',', valueStart);
        if (valueEnd == -1) valueEnd = json.indexOf('}', valueStart);
        
        // Извлекаем строку с числом
        String floatStr = json.substring(valueStart, valueEnd);
        floatStr.trim(); // Удаляем возможные пробелы
        
        // Конвертируем в float (учет как точек, так и запятых)
        *value = floatStr.toFloat();
    }
}

void updateStringIfExists(const String& json, const String& key, String* value) {
    int pos = json.indexOf(key);
    if (pos != -1) {
        int valueStart = pos + key.length() + 1; // +1 для учета кавычки
        int valueEnd = json.indexOf('"', valueStart + 1);
        if (valueEnd != -1) {
            *value = json.substring(valueStart, valueEnd);
        }
    }
}


/// Инициализация ///
void initRTCSettings() {
    if (!rtcInitialized) {
        rtcDeviceSettings = getDefaultSettings();
        rtcInitialized = true;
        Serial.println("RTC initialized with defaults");
    }
}

DeviceSettings getDefaultSettings() {
    return DeviceSettings{}; 
}


/// Работа с RTC памятью ///
bool saveSettingsToRTC(const DeviceSettings& settings) {
    memcpy(&rtcDeviceSettings, &settings, sizeof(DeviceSettings));
    return true;
}

bool loadSettingsFromRTC(DeviceSettings* settings) {
    if (!settings) return false;
    memcpy(settings, &rtcDeviceSettings, sizeof(DeviceSettings));
    return true;
}

DeviceSettings getCurrentSettings() {
    DeviceSettings settings;
    if (!loadSettingsFromRTC(&settings)) {
        settings = getDefaultSettings();
        Serial.println("Using default settings");
    }
    return settings;
}

void initDeviceSettings() {
    initRTCSettings();
    deviceSettings = getCurrentSettings();
}





// /// Инициализация ///

// // Получение текущих настроек
// void initDeviceSettings() {
//     initRTCSettings();  // Инициализация RTC

//     // Загружаем настройки (обновит оба кеша)
//     DeviceSettings loaded = getCurrentSettings();
//     deviceSettings = loaded;
//     rtcDeviceSettings = loaded;
// }

// // Инициализация RTC с дефолтными значениями
// void initRTCSettings() {
//     if (!rtcInitialized) {
//         rtcDeviceSettings = getDefaultSettings();
//         rtcInitialized = true;
//         Serial.println("RTC initialized with defaults");
//     }
// }

// // Получение текущих настроек (с единой логикой)
// DeviceSettings getCurrentSettings() {
//     DeviceSettings settings;
    
//     // Пробуем загрузить из RTC. Если нет, то используем дефолтные настройки
//     if (!loadSettingsFromRTC(&settings)) {
//         settings = getDefaultSettings();
//         Serial.println("Using default settings");
//     }

//     // // Если в RTC нет, пробуем из Flash
//     // if (!loadSettingsFromFlash(&settings)) {
//     //     // Если и в Flash нет, используем дефолтные
//     //     settings = getDefaultSettings();
//     //     Serial.println("Using default settings");
//     // }

//     return settings;
// }

// // Получение дефолтных значений из описания структуры в deviceSettings.h
// DeviceSettings getDefaultSettings() {
//     return DeviceSettings{}; 
// }



// /// Функции для загрузки и записи данный во Flash или RTC память ///

// // Унифицированная загрузка (общая логика для Flash и RTC)
// bool loadSettings(DeviceSettings* target, DeviceSettings* cache, 
//                 const char* sourceName, bool usePreferences) {
//     if (!target || !cache) return false;

//     if (usePreferences) {
//         if (!prefs.begin("device-settings", true)) {
//             Serial.println("Failed to open preferences");
//             return false;
//         }
        
//         size_t loaded = prefs.getBytes("settings", target, sizeof(DeviceSettings));
//         prefs.end();

//         if (loaded != sizeof(DeviceSettings)) {
//             Serial.println("No valid settings in Flash");
//             return false;
//         }
//     } else {
//         memcpy(target, &rtcDeviceSettings, sizeof(DeviceSettings));
//     }

//     *cache = *target; // Обновляем кеш
//     Serial.printf("Settings loaded from %s\n", sourceName);
//     return true;
// }

// // Загрузка из Flash (с обновлением кеша)
// bool loadSettingsFromFlash(DeviceSettings* settings) {
//     return loadSettings(settings, &deviceSettings, "Flash", true);
// }

// // Загрузка из RTC (с обновлением кеша)
// bool loadSettingsFromRTC(DeviceSettings* settings) {
//     return loadSettings(settings, &rtcDeviceSettings, "RTC", false);
// }




// ///  Функции для управления настройками во Flash памяти///

// // Универсальное сохранение всей структуры (только при изменениях)
// bool saveSettingsToFlash(const DeviceSettings& newSettings) {
//     // Сравниваем текущие и новые настройки побайтово
//     if (memcmp(&deviceSettings, &newSettings, sizeof(DeviceSettings)) == 0) {
//         return true; // Настройки не изменились - не сохраняем
//     }

//     if (!prefs.begin("device-settings", false)) {
//         Serial.println("Failed to open preferences");
//         return false;
//     }

//     // Сохраняем всю структуру целиком
//     size_t saved = prefs.putBytes("settings", &newSettings, sizeof(DeviceSettings));
//     prefs.end();

//     if (saved != sizeof(DeviceSettings)) {
//         Serial.println("Failed to save settings");
//         return false;
//     }

//     deviceSettings = newSettings; // Обновляем кэш
//     Serial.println("Settings saved");
//     return true;
// }

// // Универсальная загрузка
// bool loadSettingsFromFlash(DeviceSettings* settings) {
//     if (!settings) return false;
    
//     if (!prefs.begin("device-settings", true)) {
//         Serial.println("Failed to open preferences");
//         return false;
//     }

//     // Загружаем всю структуру целиком
//     size_t loaded = prefs.getBytes("settings", settings, sizeof(DeviceSettings));
//     prefs.end();

//     if (loaded != sizeof(DeviceSettings)) {
//         Serial.println("No valid settings found");
//         return false;
//     }

//     deviceSettings = *settings; // Обновляем кэш
//     return true;
// }




