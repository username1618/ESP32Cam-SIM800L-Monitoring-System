// Настройки GPRS для SIM800
struct APNSettings {
    String apn;
    String username;
    String password;

    // Метод для получения настроек APN в зависимости от оператора
    static APNSettings getAPNSettings(String networkOperator) {
        APNSettings settings;
        if (networkOperator.equals("MegaFon")) {
            settings.apn = "internet";
            settings.username = ""; 
            settings.password = "";
        } else if (networkOperator.equals("MOTIV")) {
            settings.apn = "internet.tele2.ru";
            settings.username = ""; 
            settings.password = "";
        } else if (networkOperator.equals("MTS")) {
            settings.apn = "internet.mts.ru";
            settings.username = "mts"; 
            settings.password = "mts";
        } else {
            // Обработка случая, когда оператор не распознан
            settings.apn = "internet";
            settings.username = "";
            settings.password = "";
            Serial.print("Unknown Operator: ");
            Serial.println(networkOperator);
        }
        return settings;
    }
};