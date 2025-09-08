/**
 * @file deviceSettings.h
 * @brief Управление настройками устройства с сохранением в RTC памяти
 */

typedef struct {

    // ========== Настройки устройства ==========
    int timeToSleepMinutes = 30;     // Время сна между циклами работы (в минутах)

    int flashDelay = 100;            // Задержка перед срабатыванием вспышки (в мс)
    int photoCount = 5;              // Количество снимков в одном цикле
    int photoDelay = 100;            // Задержка между снимками 

    bool returnSms = false;           // Возвращать СМС на сервер
    bool deleteSmsAfterSend = false;  // Удалять SMS после отправки
    String smsReadMode = "ALL";       // Режим чтения SMS: "ALL" или "NEW"
    int batchSms = 2;                 // Размер пакета отправки

    int sim800StartDelaySec = 60;    // Время ожидания инициализации GSM модуля (в секундах)
    int sim800TransportDelay = 15;   // Небольшая задержка каждые 1KB данных при передаче их от ESP32 в SIM800 (в мс)
    int sim800PostDelay = 1500;      // Увеличенная задержка перед выполнением POST-запроса (в мс)
    int maxRetriesToSendImage = 3;   // Максимальное количество попыток отправки изображения


    // ========== Настройки датчика напряжения ==========
    int numReadingsVoltage = 30;     // Количество измерений для усреднения
    int totalDurationSecVoltage = 3; // Общее время измерения (в секундах)

    // ========== Настройки датчика температуры и влажности ==========
    int numReadingsDHT = 30;         // Количество измерений для усреднения
    int totalDurationSecDHT = 3;     // Общее время измерения (в секундах)


    // ========== Настройки камеры ==========

    // ------ Основные параметры изображения ------
    int framesize = 6;       // Разрешение: 6=UXGA(1600x1200), 5=SXGA(1280x1024), 4=XGA(1024x768), etc.
    int pixelFormat = 0;     // Формат: 0=JPEG, 1=RGB565, 2=YUV422, 3=Grayscale
    int quality = 12;        // Качество JPEG (10-63, где 10 - наилучшее)
    int brightness = 0;      // Яркость: -2 (темнее) до +2 (ярче)
    int contrast = 0;        // Контраст: -2 (мягче) до +2 (резче)
    int saturation = 0;      // Насыщенность: -2 (ч/б) до +2 (яркие цвета)

    // ------ Настройки экспозиции и усиления ------
    bool aec = true;         // Автоэкспозиция (true=вкл, false=ручной режим)
    bool aec2 = false;       // Доп. обработка автоэкспозиции (только при aec=true)
    int aeLevel = 0;         // Коррекция автоэкспозиции: -2 (темнее) до +2 (ярче)
    int aecValue = 800;      // Значение экспозиции в ручном режиме (0-1200)
    bool agc = true;         // Автоусиление (true=вкл, false=ручной режим)
    int agcGain = 20;        // Значение усиления в ручном режиме (0-30)
    int gainCeiling = 1;     // Предел усиления: 0=2x, 1=4x, 2=8x, 3=16x, 4=32x

    // ------ Баланс белого ------
    bool awb = false;        // Автобаланс белого (true=вкл, false=ручной режим)
    bool awbGain = false;    // Усиление автобаланса белого
    int wbMode = 4;          // Режим ББ: 0=Auto, 1=Sunny, 2=Cloudy, 3=Office, 4=Home

    // ------ Коррекция изображения ------
    bool bpc = true;         // Коррекция черных пикселей (подавление шумов)
    bool wpc = true;         // Коррекция белых пикселей (анти-пересвет)
    bool rawGma = true;      // Гамма-коррекция (улучшение деталей в тенях)
    bool lensCorrection = false; // Коррекция дисторсии линзы (лучше отключать на AI Thinker)
    bool dcw = true;         // Цифровое масштабирование / Downscale (вкл для разрешений >SVGA)

    // ------ Эффекты и ориентация ------
    int specialEffect = 0;   // Спецэффекты: 0=None, 1=Negative, 2=Grayscale, etc.
    bool hmirror = false;    // Горизонтальное зеркало
    bool vflip = false;      // Вертикальное отражение

    // ------ Аппаратные фиксы (для ESP32-CAM AI Thinker) ------
    bool bankSelFix = true;     // Фикс выбора банка регистров (0xFF=0x01)
    bool clockSpeedFix = true;  // Настройка тактовой частоты (0x11=0x01)
    bool hrefFix = true;        // Фикс обрезки HREF (0x15=0x00)
    bool jpegQualityFix = true; // Фикс артефактов JPEG (0x44=0x06)
    bool pinkTintFix = true;    // Фикс розового оттенка (последовательность 0x7C/0x7D)

} DeviceSettings;

// Глобальный экземпляр
extern DeviceSettings deviceSettings;


String extractJson(String response);
bool parseCameraSettings(String json, DeviceSettings* settings);
void updateBoolIfExists(const String& json, const String& key, bool* value);
void updateIntIfExists(const String& json, const String& key, int* value);
void updateFloatIfExists(const String& json, const String& key, float* value);
void updateStringIfExists(const String& json, const String& key, String* value);


void initRTCSettings();
DeviceSettings getDefaultSettings();
bool saveSettingsToRTC(const DeviceSettings& settings);
bool loadSettingsFromRTC(DeviceSettings* settings);
DeviceSettings getCurrentSettings();
void initDeviceSettings();
