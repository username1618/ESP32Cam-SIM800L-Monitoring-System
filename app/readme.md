## Комплектующие:
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

Питание от АКБ с датчиком напряжения:
• USB Кабель питания с дата-линиями (https://www.ozon.ru/product/kabel-micro-usb-kabel-dlya-zaryadki-shnur-dlya-zaryadki-2-metra-emyx-1001044602/)
• Понижающий модуль DC-DC LM2596S (https://www.ozon.ru/product/reguliruemyy-ponizhayushchiy-modul-dc-dc-lm2596s-preobrazovatel-vstraivaemyy-621665954/?at=OgtE1WyzqTryJRgXUo50L2RU28QgxrSA62GvMFM9j11N)
• Гнездо питания USB (https://www.ozon.ru/product/gnezdo-pitaniya-usb-ustanovochnoe-na-korpus-4pin-zashchelki-dlina-105mm-1210637303/)
• Датчик напряжения (https://www.ozon.ru/product/datchik-napryazheniya-do-25-v-313926650/) или собрать свой делитель напряжения на резисторах.
• Клеммы аккумуляторные автомобильные быстросъёмные(https://www.ozon.ru/product/klemmy-akkumulyatornye-avtomobilnye-bystrosemnye-izolirovannye-dlya-bystrogo-montazha-2-sht-1216196687/)

Питание от PowerBank:
• USB Кабель питания без дата-линиий (https://www.ozon.ru/product/kabel-dlya-zaryadki-i-peredachi-dannyh-supertolstyy-bold-micro-usb-chernyy-3a-tpe-1-m-smartbuy-1525584666/)

Датчики:
• Датчик влажности и температуры DHT11 (https://www.ozon.ru/product/datchik-vlazhnosti-i-temperatury-dht11-arduino-1592869916/?at=oZt62O0jxtEkrnKOTEx2polfrOnm4Ks0x56rRT9mrwJY)
• Инфракрасный датчик движения AM312 (https://www.ozon.ru/product/infrakrasnyy-datchik-dvizheniya-am312-dlya-arduino-1-sht-1739793295/)

Опционально:
• Динамик
• Микрофон



## Miniconda
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-aarch64.sh
bash Miniconda3-latest-Linux-aarch64.sh
source ~/.bashrc

# Create env with Python 3.8.5 (на более поздних версиях python, sim800 не отправляет запросы по https т.к. в python изменились поддерживаемые версии TLS). 
conda create -n py385 python=3.8.5 -c conda-forge
conda activate py385



# Fix pip network error
sudo nano /etc/resolv.conf
nameserver 8.8.8.8
nameserver 8.8.4.4

# Fix apt update error
sudo nano /etc/apt/sources.list
deb http://ftp.ru.debian.org/debian bookworm main contrib non-free
deb http://ftp.ru.debian.org/debian bookworm-updates main contrib non-free
deb http://ftp.ru.debian.org/debian-security bookworm-security main contrib non-free



# создание и изменение владельца директории
sudo mkdir /opt/srv/app
sudo chown -R davinci1618:davinci1618 /opt/srv/app



# Установка прав на выполнение скриптов сборки образов
chmod +x build-sim800.sh build-web.sh

# Запуск сборки
#dos2unix build-sim800.sh
#dos2unix build-web.sh
./build-sim800.sh
./build-web.sh




## 1. Устанавливаем через официальный скрипт
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh ./get-docker.sh

## 2. Добавляем пользователя в группу docker
sudo usermod -aG docker $USER
newgrp docker



# Настройка для reddis
echo "vm.overcommit_memory = 1" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p

# Включим поддержку cgroups в ядре
sudo nano /boot/firmware/cmdline.txt
>>cgroup_enable=memory cgroup_memory=1
sudo reboot

# Настройка SWAP в системе (для страховки):
После применения этой конфигурации запись на SD-карту будет минимальной, что значительно продлит её срок службы.
sudo dphys-swapfile swapoff
sudo nano /etc/dphys-swapfile  # CONF_SWAPSIZE=512
sudo dphys-swapfile setup
sudo dphys-swapfile swapon




## 3. Build docker images
bash build-sim800.sh
bash build-web.sh



## 4. Run services
docker compose up -d --force-recreate


# Мониторинг использования памяти: redis
watch -n 5 "free -m && docker stats redis --no-stream"
