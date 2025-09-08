# ESP32-CAM + SIM800L: Умная камера с GSM связью 🚀📡


## Автономная IoT-камера с продвинутым энергосбережением и передачей данных через GPRS/HTTP/FTP


## 🌟 Ключевые возможности
* 📷 Интеллектуальная съемка
  - Автонастройка параметров камеры (экспозиция, баланс белого, фильтры)
  - Поддержка разрешений до 1600x1200 с аппаратными фиксами для AI-Thinker

* ⚡ Ультра-низкое потребление
  - Гибридный режим сна (глубокий/легкий сон)
  - До нескольких лет работы от батареи 18650 в режиме ожидания!
  - Автоматическое пробуждение по таймеру / PIR


* 📡 Надежная передача данных
  - Двойной режим отправки: HTTP и/или FTP
  - Повторная отправка при сбоях (до 3 попыток)
  - СМС-оповещения и голосовые вызовы при тревоге

* 🧠 Самонастраиваемая система
  - Динамическое обновление параметров с сервера
  - Автоподбор APN для любого оператора связи


## 🛠 Аппаратная часть
* ESP32-CAM (AI-Thinker)
* Модуль SIM800L
* Датчик DHT11 и делитель напряжения для АКБ
* Источник питания 3.7-5В с пиковым током 2А+
• ESP32-CAM (OV2640, AI_THINKER) (https://www.ozon.ru/product/modul-esp32-cam-s-kameroy-1727032723/?at=MZtvBNAMEt7PVg0jHwMZLrDFxjZMOGIWK0n0VHDqpVDK)
• SIM800L (https://www.ozon.ru/product/modul-gsm-gprs-sim800l-s-antennoy-933031795/)
• TTL UART программатор USB CP2102 (https://www.ozon.ru/product/adapter-perehodnik-usb-v-uart-ttl-s-provodami-na-baze-cp2102-1298477376/?at=z6tOW5AQJcwgw45Lu77EBNPs1jB9JKiQQ1NoYTVBOKZy)
• Понижающий преобразователь GSMIN MP1584EN DC-DC (https://www.ozon.ru/product/ponizhayushchiy-preobrazovatel-napryazheniya-gsmin-mp1584en-dc-dc-2sht-zelenyy-1563527322/?from_sku=1563527322&oos_search=false)
• Разъем SMA-female для Антенны (https://www.ozon.ru/product/megafon-antenna-dlya-setevogo-oborudovaniya-lte-usb-modem-with-wi-fi-hotspot-1752053568/)
• Антенна GSM (https://www.ozon.ru/product/antenna-gsm-dlya-sotovogo-signala-routera-sma-orbita-ot-gsm27-1705559235/)
• Макетная плата 40x60 (https://www.ozon.ru/product/maketnaya-plata-40x60-dvustoronnyaya-5-shtuk-303825732/?at=BrtzWYOpEsXgl5kkf0PP2YJunQN6LQTVM6JkXhzNWLXr)
• PLD разъемы (https://www.ozon.ru/product/shtyrevye-odnoryadnye-razemy-na-platu-mama-pbs-40-40-pin-2-54-mm-10-shtuk-pld-razem-dlya-proektov-1051231516/?at=Z8tXKJprOUKrwDWLH2YLpY1F1lVKvlcBP2oJkuxOPooq)
• SIM-карта (https://www.ozon.ru/product/sim-karta-t2-10-gb-novyy-nomer-nepublichnyy-tarif-dlya-modema-routera-smartfona-plansheta-1766520536/?oos_search=false&prev_collection=34135299)
• Набор резисторов
• Конденсаторы
• Провода для пайки
• Макетные провода
• Термоусадки
• Припой (https://www.ozon.ru/product/pripoy-dlya-payki-pos-61-s-kanifolyu-diametr-0-8-mm-100-gr-provoloka-na-katushke-trubka-olovo-601139208/)
• Флюс (https://www.ozon.ru/product/flyus-dlya-montazha-smd-bga-soyuz-apollon-6-412-a-rusflux-10ml-864411396/)

Питание от PowerBank:
• USB Кабель питания без дата-линиий (https://www.ozon.ru/product/kabel-dlya-zaryadki-i-peredachi-dannyh-supertolstyy-bold-micro-usb-chernyy-3a-tpe-1-m-smartbuy-1525584666/)

Питание от АКБ с датчиком напряжения:
• USB Кабель питания с дата-линиями (https://www.ozon.ru/product/kabel-micro-usb-kabel-dlya-zaryadki-shnur-dlya-zaryadki-2-metra-emyx-1001044602/)
• Понижающий модуль DC-DC LM2596S (https://www.ozon.ru/product/reguliruemyy-ponizhayushchiy-modul-dc-dc-lm2596s-preobrazovatel-vstraivaemyy-621665954/?at=OgtE1WyzqTryJRgXUo50L2RU28QgxrSA62GvMFM9j11N)
• Гнездо питания USB (https://www.ozon.ru/product/gnezdo-pitaniya-usb-ustanovochnoe-na-korpus-4pin-zashchelki-dlina-105mm-1210637303/)
• Датчик напряжения (https://www.ozon.ru/product/datchik-napryazheniya-do-25-v-313926650/) или собрать свой делитель напряжения на резисторах.
• Клеммы аккумуляторные автомобильные быстросъёмные(https://www.ozon.ru/product/klemmy-akkumulyatornye-avtomobilnye-bystrosemnye-izolirovannye-dlya-bystrogo-montazha-2-sht-1216196687/)

Датчики:
• Датчик влажности и температуры DHT11 (https://www.ozon.ru/product/datchik-vlazhnosti-i-temperatury-dht11-arduino-1592869916/?at=oZt62O0jxtEkrnKOTEx2polfrOnm4Ks0x56rRT9mrwJY)
• Инфракрасный датчик движения AM312 (https://www.ozon.ru/product/infrakrasnyy-datchik-dvizheniya-am312-dlya-arduino-1-sht-1739793295/)

Опционально:
• Динамик
• Микрофон

### Схема подключения
```cpp
// Порт UART2 для связи с GSM
Serial2.begin(115200, SERIAL_8N1, 14, 15); 

// Ключевые пины:
#define GSM_RESET_PIN 2    // Reset SIM800L
#define FLASH_LED_PIN 4    // Вспышка камеры
#define VOLT_SENSOR_PIN 12 // Датчик напряжения АКБ
#define DHT_PIN 13         // Датчик температуры и влажности
```

### Советы по питанию:

ESP32 Cam и SIM800L питаются отдельно. SIM800L может потреблять до 2А в пиковых значениях, и если источник питания не может обеспечить такой ток, он не сможет подключиться к сети. Также убедитесь, что провода, питающие SIM800L, достаточно толстые и не слишком длинные (например, провода, которые обычно используются для соединений с золотыми контактами, слишком тонкие).
Я использовал два отдельных понижающих регулируемых DC/DC преобразователя, которые позволяют подавать точное напряжение на каждый модуль. Питание SIM800L напрямую от аккумулятора на 3,7 В также возможно, однако, когда напряжение падает примерно до 3,6 В, у SIM800L могут возникнуть проблемы с подключением к GSM-сети. Решением этой проблемы может быть добавление конденсатора на 1000 мкФ, подключенного к VCC и GND SIM800L. В любом случае, кажется, что лучшее решение — питать SIM800L от источника с более высоким напряжением (например, два аккумулятора по 3,7 В в последовательном соединении), понижая выходное напряжение до 3,8 - 4,2 В с помощью понижающего DC-регулятора.

✅ Используйте отдельные DC-DC преобразователи для ESP32 и SIM800L

✅ Добавьте конденсатор 1000мкФ к питанию GSM модуля

✅ Оптимальное напряжение: 4.0В ±5% для SIM800L


## 💻 Программные особенности
Динамическая конфигурация:
```cpp
// Пример структуры настроек в RTC-памяти
struct DeviceSettings {
    int sleep_time = 30;    // минуты
    uint8_t jpeg_quality = 12;
    bool pinkTintFix = true;// Аппаратный фикс оттенков
    // ...50+ параметров
};
```

### Фишки реализации:
* Медианная фильтрация замеров напряжения и температуры
* Квантование ADC-замеров для снижения шумов
* Автоматический рестарт GSM при зависании
* Адаптивный размер пакетов при FTP-передаче


## 📄 Лицензия
MIT License
