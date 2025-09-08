#include "Sim800lClient.h"



Sim800lClient::Sim800lClient(void){
}

void Sim800lClient::resetGsm(int resetPinNum){
    if (DEBUG_LOGS) {
      Serial.println("Resetting GSM module ...");
    }
    digitalWrite(resetPinNum, LOW);
        delay(500);
    digitalWrite(resetPinNum, HIGH);  
}  

boolean Sim800lClient::waitForGsmNetwork(){
  sendATcommand("AT","OK", 5000);
  String result;

  if (DEBUG_LOGS) {
    result = sendATcommand("AT+CMEE=2", "OK", 10000);
      if (result.indexOf("OK") == -1) {
        Serial.println("Failed to set extra logs mode");
      }
      result =  sendATcommand("AT+GMR","OK", 5000); // версия прошивки SIM800 (1418B05SIM800L24)
  }

  for (int i=1; i < 10; i++){ //Try 10 times…
      if (DEBUG_LOGS) {
        Serial.println("Waiting for network, attempt " + String(i) );
      }
      result = sendATcommand("AT+CREG?","+CREG: 0,1", 5000);
      if (result.indexOf("+CREG: 0,1") > 0) {
        return true;
      }
      delay(2000);
  }

  return false;
}

boolean Sim800lClient::goToSleep(){
  String result = sendATcommand("AT+CSCLK=2","OK", 5000);
  if (result.indexOf("OK") <= 0) {
      if (DEBUG_LOGS) {
        Serial.println("Cannot set SIM800l to sleep mode");
      }
      return false;
  }
  return true;
}


boolean Sim800lClient::initFtp(String _ftpServerAddress, int _ftpServerPort, String _ftpUser, String _ftpPassword) {
  ftpServerAddress = _ftpServerAddress;
  ftpServerPort =  _ftpServerPort;
  ftpUser = _ftpUser;
  ftpPassword = _ftpPassword;
  String response = sendATcommand("AT+SAPBR=1,1","OK", 10000);
  if (response.indexOf("OK") <= 0) {
    return false;
  }

  sendATcommand(String("AT+FTPSERV=")  + "\"" + ftpServerAddress + "\"","OK", 5000);
  sendATcommand(String("AT+FTPPORT=")  + String(ftpServerPort),"OK", 5000);
  sendATcommand(String("AT+FTPUN=")  + "\"" + ftpUser + "\"" ,"OK", 5000);
  response = sendATcommand(String("AT+FTPPW=")  + "\"" + ftpPassword + "\"","OK", 5000);

  if (response.indexOf("OK") > 0) {
      return true;
    } else {
      if (DEBUG_LOGS) {
        Serial.println("FTP login error, response: " + response);
      }
      return false;
    }
}

void Sim800lClient::stopFtp(void){
  String response = sendATcommand("AT+FTPQUIT","OK", 5000);
  if (response.indexOf("OK") <= 0) {
    if (DEBUG_LOGS) {
      Serial.println("Cannot close FTP session");
    }
  }
  
  response = sendATcommand("AT+SAPBR=0,1","OK", 5000);
  if (response.indexOf("OK") <= 0) {
    if (DEBUG_LOGS) {
      Serial.println("Cannot close network connection");
    }
  }
}

boolean Sim800lClient::sendFileToFtp(camera_fb_t * fb, String remoteFileName){
  String response = sendATcommand(String("AT+FTPPUTNAME=") +  "\"" + remoteFileName + "\"" ,"OK",10000);
  response = sendATcommand("AT+FTPCID=1" ,"OK",10000);
  response = sendATcommand("AT+FTPPUTPATH=\"./ftp/\"" ,"OK",10000);
  response = sendATcommand("AT+FTPPUT=1","+FTPPUT: 1",60000);
  String putOkResp = "+FTPPUT: 1,1,";
  int putOkRespIndex = response.indexOf(putOkResp);
  if ( putOkRespIndex <= 0) {
    blinkRed(5, 200, 200);
    if (DEBUG_LOGS) {
      Serial.println("FTP PUT error: " + response);
    }
    return false;
    }
  String maxLengthString = response.substring(putOkRespIndex + putOkResp.length());
  int bufLength = maxLengthString.toInt();

  // Установите фиксированный размер батча, например, 512 байт
  const int fixedBatchSize = 256;
  if (bufLength > fixedBatchSize) {
      bufLength = fixedBatchSize;
  }


    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    int page=0;

    if (DEBUG_LOGS) {
      Serial.println("Data length : " + String(fbLen));
      Serial.println("Data buffer length : " + String(bufLength));
    }
    
    for (size_t n=0; n<fbLen; n=n+bufLength) {
      page++;
      if ( n+bufLength > fbLen ) {
        bufLength = fbLen%bufLength;
      }
      if (! sendDataPage(fbBuf, bufLength)) {
           return false;
       }
      fbBuf += bufLength;
      if (DEBUG_LOGS) {
        Serial.println("Page no: " + String(page) + " sent, bytes: " + String(n+bufLength) + "/" + fbLen);
      }
      String sendResp = readLineFromSerial("OK", 60000);
      if (sendResp.indexOf("OK") <= 0 ) {
        if (DEBUG_LOGS) {
          Serial.println("Page NOT confirmed, reporting error"); 
        }
        return false;
      }  
    }   

  response = sendATcommand(String("AT+FTPPUT=2,0"),"OK", 5000); 
  if (response.indexOf("OK") <= 0) {
      if (DEBUG_LOGS) {
        Serial.println("Error while closing FTP transfer");
      }
      return false;
  }
  if (DEBUG_LOGS) {
    Serial.println("Picture sent to FTP");
  }
  blinkRed(1, 1000, 0);
  return true;

}

boolean Sim800lClient::sendDataPage(uint8_t *fbBuf, size_t len){
   if (DEBUG_LOGS) {
     Serial.println(String("Sending next page of data with length: ") + String(len));
   }
   String response = sendATcommand(String("AT+FTPPUT=2,") + len, String("+FTPPUT: 2,") + len,5000); 
   if (response.indexOf("+FTPPUT: 2,") <= 0) {
      if (DEBUG_LOGS) {
        Serial.println("Error sending data page"); 
      }
      return false;
   }
   Serial2.write(fbBuf, len);
}




boolean Sim800lClient::startHttp(ApiParams* api, APNSettings* apn){
    // Устанавливаем параметры GPRS-соединения
    String response = sendATcommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", "OK", 10000);
    if (response.indexOf("OK") == -1) {
        if (DEBUG_LOGS) {
          Serial.println("Failed to set GPRS Contype");
        }
        return false;
    }

    // Устанавливаем APN
    response = sendATcommand("AT+SAPBR=3,1,\"APN\",\"" + apn->apn + "\"", "OK", 10000);
    if (response.indexOf("OK") == -1) {
        if (DEBUG_LOGS) {
          Serial.println("Failed to set GPRS APN");
        }
        return false;
    }
    // Если необходимо, можно установить имя пользователя и пароль
    if (apn->username.length() > 0) {
        response = sendATcommand("AT+CSTT=\"" + apn->apn + "\",\"" + apn->username + "\",\"" + apn->password + "\"", "OK", 10000);
        if (response.indexOf("OK") == -1) {
            if (DEBUG_LOGS) {
              Serial.println("Failed to set GPRS credentials");
            }
            return false;
        }
    }

    response = sendATcommand("AT+SAPBR=1,1", "OK", 20000);
    if (response.indexOf("OK") == -1) {
        if (DEBUG_LOGS) {
          Serial.println("Failed to start GPRS");
        }
        return false;
    }
    response = sendATcommand("AT+SAPBR=2,1", "OK", 20000);
    if (response.indexOf("OK") == -1) {
        if (DEBUG_LOGS) {
          Serial.println("Failed to check GPRS");
        }
        return false;
    }

    // Проверка подключения к GPRS
    response = sendATcommand("AT+CGATT?", "1", 10000);
    if (response.indexOf("1") == -1) {
        if (DEBUG_LOGS) {
          Serial.println("GPRS not attached");
        }
        return false;
    }

    response = sendATcommand("AT+CGREG?", "OK", 5000);
    if (DEBUG_LOGS) {
      Serial.println("GPRS Network Registration Status: " + response);
    }



    // Инициализация HTTP
    response = sendATcommand("AT+HTTPINIT", "OK", 10000);
    if (response.indexOf("OK") == -1) {
        if (DEBUG_LOGS) {
          Serial.println("Failed to initialize HTTP");
        }
        return false;
    }

    // Настройка HTTPS в зависимости от режима (у SIM800L устаревшая версия TLS)
    response = sendATcommand(api->useHttps ? "AT+HTTPSSL=1" : "AT+HTTPSSL=0", "OK", 10000);
    if (response.indexOf("OK") == -1) {
        if (DEBUG_LOGS) Serial.println("Failed to set SSL mode");
        return false;
    }

    // Установка CID (профиль оператора)
    response = sendATcommand("AT+HTTPPARA=\"CID\",1", "OK", 10000);
    if (response.indexOf("OK") == -1) {
        if (DEBUG_LOGS) {
          Serial.println("Failed to set CID");
        }
        return false;
    }

    return true;

}

void Sim800lClient::stopHttp(void){
  // Завершение HTTP-сессии
  String response = sendATcommand("AT+HTTPTERM", "OK", 10000);
  if (response.indexOf("OK") <= 0) {
    if (DEBUG_LOGS) {
      Serial.println("Can't close HTTP session");
    }
  }

  response = sendATcommand("AT+SAPBR=0,1", "OK", 10000);
  if (response.indexOf("OK") <= 0) {
    if (DEBUG_LOGS) {
      Serial.println("Can't close HTTP session");
    }
  }
}



bool Sim800lClient::getCameraSettingsHttp(ApiParams* api, DeviceSettings* settings) {

    // Установка URL
    String urlCommand = "AT+HTTPPARA=\"URL\",\"" + api->getSettingsUrl() + "\"";
    if (sendATcommand(urlCommand.c_str(), "OK", 10000).indexOf("OK") == -1) {
        if (DEBUG_LOGS) {
          Serial.println("URL set failed");
        }
        return false;
    }

    // Выполнение GET-запроса
    String actionResponse = sendATcommand("AT+HTTPACTION=0", "+HTTPACTION:", 60000);
    
    // Проверка кода ответа
    int statusCode = parseStatusCode(actionResponse);
    if (statusCode != 200) {
        if (DEBUG_LOGS) {
          Serial.println("HTTP error: " + String(statusCode));
        }
        return false;
    }

    // Чтение данных
    String jsonResponse = sendATcommand("AT+HTTPREAD", "OK", 60000);

    // Препроцессинг JSON
    String cleanJson = extractJson(jsonResponse);
    if (cleanJson.length() == 0) {
        if (DEBUG_LOGS) {
          Serial.println("Invalid JSON response");
        }
        return false;
    }

    // Парсинг настроек
    return parseCameraSettings(cleanJson, settings);
}

int Sim800lClient::parseStatusCode(String response) {
    int commaPos = response.indexOf(',');
    if (commaPos != -1) {
        return response.substring(commaPos + 1, commaPos + 4).toInt();
    }
    return 0;
}


boolean Sim800lClient::sendFileByHttp(camera_fb_t * fb, String fileName, ApiParams* api) {

    const String boundary = "myboundary";
    String headers = "--" + boundary + "\r\n";
    headers += "Content-Disposition: form-data; name=\"file\"; filename=\"" + fileName + "\"\r\n";
    headers += "Content-Type: image/jpeg\r\n\r\n";
    String end = "\r\n--" + boundary + "--\r\n";
    
    size_t totalLength = headers.length() + fb->len + end.length();

    // Установка типа контента
    String contentTypeCommand = "AT+HTTPPARA=\"CONTENT\",\"multipart/form-data; boundary=" + boundary + "\"";
    String response = sendATcommand(contentTypeCommand.c_str(), "OK", 10000);
    if (response.indexOf("OK") == -1) {
        if (DEBUG_LOGS) {
          Serial.println("Failed to set content type");
        }
        return false;
    }

    // Установка URL
    String urlCommand = "AT+HTTPPARA=\"URL\",\"" + api->uploadUrl() + "\"";
    response = sendATcommand(urlCommand.c_str(), "OK", 10000);
    if (response.indexOf("OK") == -1) {
        if (DEBUG_LOGS) {
          Serial.println("Failed to set URL");
        }
        return false;
    }

    // Подготовка к отправке данных
    if (DEBUG_LOGS) {
      Serial.println("Sending image...");
    }
    String dataCommand = "AT+HTTPDATA=" + String(totalLength) + ",30000";
    response = sendATcommand(dataCommand.c_str(), "DOWNLOAD", 30000);
    if (response.indexOf("DOWNLOAD") == -1) {
        if (DEBUG_LOGS) {
          Serial.println("Failed to prepare for data sending");
        }
        return false;
    }

    // Отправка заголовков и данных изображения
    Serial2.print(headers);
    for (size_t i = 0; i < fb->len; i++) {
        Serial2.write(fb->buf[i]);
        if (i % 1024 == 0) delay(deviceSettings.sim800TransportDelay); // Небольшая задержка каждые 1KB данных
    }
    Serial2.print(end);
    
    delay(deviceSettings.sim800PostDelay); // Увеличенная задержка перед выполнением POST-запроса
    

    // Выполнение HTTP-запроса
    response = sendATcommand("AT+HTTPACTION=1", "+HTTPACTION:", 120000); // 1 - POST
    if (response.indexOf("+HTTPACTION:") != -1) {
        int statusCode = response.substring(response.indexOf(",") + 1).toInt();
        if (statusCode == 200) {
            if (DEBUG_LOGS) {
              Serial.println("POST request successful");
            }
            return true;
        } else {
            if (DEBUG_LOGS) {
              Serial.println("POST request failed with status code: " + String(statusCode));
            }
            return false;
        }
    } else {
        if (DEBUG_LOGS) {
          Serial.println("POST request failed: " + response);
        }
        return false;
    }

    return true; 
}


void Sim800lClient::sendSMS(String number, String message) {
    // Установка текстового режима
    if (sendATcommand("AT+CMGF=1", "OK", 10000)) {
        if (DEBUG_LOGS) {
          Serial.println("Текстовый режим установлен.");
        }
    } else {
        if (DEBUG_LOGS) {
          Serial.println("Ошибка установки текстового режима.");
        }
        return;
    }

    // Отправка SMS
    String command = "AT+CMGS=\"" + number + "\"";  // Формат команды для отправки SMS
    if (sendATcommand(command, ">", 10000)) {
        Serial2.print(message);  // Отправка текста сообщения
        Serial2.write(26);  // Отправка символа Ctrl+Z для завершения сообщения
        if (DEBUG_LOGS) {
          Serial.println("Сообщение отправлено на " + number);
        }
    } else {
        if (DEBUG_LOGS) {
          Serial.println("Ошибка при попытке отправить SMS на " + number);
        }
    }
}

void Sim800lClient::makeCall(String number) {
    String command = "ATD" + number + ";";  // Формат команды для звонка
    if (sendATcommand(command, "OK", 10000)) {
      if (DEBUG_LOGS) {
        Serial.println("Звонок начат на " + number);
      }
    } else {
      if (DEBUG_LOGS) {
        Serial.println("Ошибка при попытке позвонить на " + number);
      }
    }
}

void Sim800lClient::waitForCallToEnd() {
    if (DEBUG_LOGS) {
      Serial.println("Ожидание завершения звонка...");
    }
    unsigned long startTime = millis();
    while (true) {
        String response = sendATcommand("AT+CLCC", "OK", 5000);
        if (response.indexOf("NO CARRIER") != -1) {
            if (DEBUG_LOGS) {
              Serial.println("Звонок завершен (NO CARRIER)");
            }
            break;
        } else if (response.indexOf("+CLCC:") == -1 && response.indexOf("OK") != -1) {
            if (DEBUG_LOGS) {
              Serial.println("Звонок завершен (нет активных вызовов)");
            }
            break;
        } else if (response.indexOf("+CLCC:") != -1) {
          if (DEBUG_LOGS) {
            Serial.println("Вызов активен..." );
          }
        }
        delay(1000);
    }
    if (DEBUG_LOGS) {
      Serial.println("Таймаут ожидания завершения звонка");
    }
}


String Sim800lClient::getBatteryVoltage(void) {
  String response = sendATcommand("AT+CBC","OK", 5000); 
  int voltStartIndex = response.lastIndexOf(",") + 1;
  String voltageStr = response.substring(voltStartIndex, voltStartIndex + 4);
  float voltageFloat = voltageStr.toFloat() / 1000;
  if (DEBUG_LOGS) {
    Serial.println("Voltage: " + String(voltageFloat, 2));
  }
  return voltageStr;
}

String Sim800lClient::getSignalStrength(void) {
  String response =   sendATcommand("AT+CSQ","+CSQ:", 5000);
  int startIndex = response.lastIndexOf(":") + 2;
  String signalStr = response.substring(startIndex, startIndex + 2);
  if (DEBUG_LOGS) {
    Serial.println("Signal Strength: " + signalStr);
  }
  return signalStr;
}

String Sim800lClient::getNetworkOperator(void) {
    String response = sendATcommand("AT+COPS?", "OK", 20000);

    // Извлекаем название оператора
    int operatorStartIndex = response.indexOf("\"") + 1;
    int operatorEndIndex = response.indexOf("\"", operatorStartIndex);
    String operatorName = response.substring(operatorStartIndex, operatorEndIndex);
    
    if (DEBUG_LOGS) {
      Serial.println("Network Operator: " + operatorName);
    }
    return operatorName;
}

String Sim800lClient::getCurrentDateTime() {
    // Отправляем команду для получения текущей даты и времени
    String response = sendATcommand("AT+CCLK?", "+CCLK:", 5000);
    
    if (response.indexOf("+CCLK:") != -1) {
        // Извлекаем строку с датой и временем
        int startIndex = response.indexOf("+CCLK:") + 7; // 7 - длина "+CCLK: "
        int endIndex = response.indexOf("\r", startIndex); // Находим конец строки
        if (endIndex == -1) endIndex = response.length();
        
        String dateTime = response.substring(startIndex, endIndex);
        
        // Удаляем кавычки
        dateTime.replace("\"", "");
        
        // Проверяем формат и исправляем при необходимости
        if (dateTime.length() >= 17) {
            // Формат: YY/MM/DD,HH:MM:SS+TZ
            String year = dateTime.substring(0, 2);
            String month = dateTime.substring(3, 5);
            String day = dateTime.substring(6, 8);
            String hour = dateTime.substring(9, 11);
            String minute = dateTime.substring(12, 14);
            String second = dateTime.substring(15, 17);
            String tz = "";
            
            if (dateTime.length() > 17) {
                tz = dateTime.substring(17);
                // Форматируем часовой пояс
                if (tz.length() == 2) {
                    tz = "+" + tz + ":00";
                } else if (tz.length() > 2) {
                    tz = tz;
                }
            } else {
                tz = "+00:00";
            }
            
            // Формируем корректную временную метку в формате ISO 8601
            String formattedDateTime = "20" + year + "-" + month + "-" + day + "T" + 
                                     hour + ":" + minute + ":" + second + tz;
            
            if (DEBUG_LOGS) {
                Serial.println("Current date and time: " + formattedDateTime);
            }
            return formattedDateTime;
        }
    }
    
    if (DEBUG_LOGS) {
        Serial.println("Failed to get date and time");
    }
    
    // Возвращаем текущее время в формате ISO 8601
    return "2000-01-01T00:00:00+00:00";
}

String Sim800lClient::readLineFromSerial(String stringToRead, unsigned long timeoutMillis){
    String result;
    unsigned long startTime = millis();
    boolean ok = false;
    boolean timeoutReached = false;
    if (DEBUG_LOGS) {
      Serial.print("Received: ");
    }
    while (!ok & !timeoutReached) {
     if (Serial2.available()) {
       String s = Serial2.readString();
       ok = s.indexOf(stringToRead) > 0;
       if (DEBUG_LOGS) {
         Serial.print(s);
       }
       result += s;  // append to the result string
     }
     timeoutReached = millis() - startTime > timeoutMillis;
   }
   if (DEBUG_LOGS) {
    Serial.println("");
   }
   if (timeoutReached) {
    if (DEBUG_LOGS) {
      Serial.println("Timeout detected after waiting for " + String(timeoutMillis) + " milliseconds");
    }
   } 
   return result;
  }

String Sim800lClient::sendATcommand(String toSend, String expectedResponse, unsigned long milliseconds) {
  if (DEBUG_LOGS) {
    Serial.println("Sending AT command: [" + toSend + "]" 
      + " , expect : [" + expectedResponse + "]" 
      + " in " + String(milliseconds) + " milliseconds");
  }
  Serial2.println(toSend);
  String result = readLineFromSerial(expectedResponse, milliseconds);
  return result;
}


void Sim800lClient::blinkRed(int count, int onTime, int offTime) {
  for (int i = 1; i <= count; i++) {
    digitalWrite(33, LOW); // RED diode on 
    delay(onTime);
    digitalWrite(33, HIGH); // RED diode off 
    delay(offTime);
  }  
}


std::vector<SmsMessage> Sim800lClient::getNewSms() {
    std::vector<SmsMessage> messages;
    
    // Установка текстового режима
    if (sendATcommand("AT+CMGF=1", "OK", 10000)) {
        if (DEBUG_LOGS) {
            Serial.println("Текстовый режим установлен для чтения СМС.");
        }
    } else {
        if (DEBUG_LOGS) {
            Serial.println("Ошибка установки текстового режима для СМС.");
        }
        return messages;
    }
    
    if (DEBUG_LOGS) {
        // Получаем количество SMS
        String smsCountResponse = sendATcommand("AT+CPMS?", "OK", 5000);
        Serial.println("Ответ на AT+CPMS?: " + smsCountResponse);
        // Пример ответа: +CPMS: "SM",2,10,"SM",2,10,"SM",2,10
        // Где 2 - количество SMS, 10 - емкость
    }
    
    // Определяем команду для чтения SMS в зависимости от режима
    String readCommand;
    if (deviceSettings.smsReadMode == "NEW") {
        readCommand = "AT+CMGL=\"REC UNREAD\"";
        if (DEBUG_LOGS) {
            Serial.println("Чтение только новых (непрочитанных) СМС");
        }
    } else {
        // По умолчанию читаем все SMS
        readCommand = "AT+CMGL=\"ALL\"";
        if (DEBUG_LOGS) {
            Serial.println("Чтение всех СМС");
        }
    }


    // Получаем SMS в соответствии с выбранным режимом
    String response = sendATcommand(readCommand, "OK", 10000);
    if (DEBUG_LOGS) {
        Serial.println("Полный ответ на " + readCommand + ": " + response);
    }
    
    // Проверяем, есть ли в ответе данные о СМС
    if (response.indexOf("+CMGL:") != -1) {
        if (DEBUG_LOGS) {
            Serial.println("Обнаружены СМС в ответе, начинаем парсинг...");
        }
        
        // Парсим ответ
        int index = 0;
        while ((index = response.indexOf("+CMGL:", index)) != -1) {
            // Пример строки: +CMGL: 1,"REC READ","651","","25/03/10,11:33:39+12"
            int start = index + 7; // длина "+CMGL:"
            int end = response.indexOf("\r\n", start);
            if (end == -1) end = response.length();
            
            String header = response.substring(start, end);
            
            if (DEBUG_LOGS) {
                Serial.println("Обрабатываем заголовок: " + header);
            }
            
            // Извлечение индекса сообщения
            int commaPos = header.indexOf(",");
            if (commaPos == -1) {
                index += 7; // Пропускаем +CMGL:
                continue;
            }
            
            int msgIndex = header.substring(0, commaPos).toInt();
            
            // Извлечение статуса
            int statusStart = header.indexOf("\"", commaPos);
            int statusEnd = -1;
            String status = "";
            if (statusStart != -1) {
                statusStart += 1; // Пропускаем открывающую кавычку
                statusEnd = header.indexOf("\"", statusStart);
                if (statusEnd != -1) {
                    status = header.substring(statusStart, statusEnd);
                }
            }
            
            // Извлечение номера телефона
            String phoneNumber = "";
            int phoneStart = -1;
            int phoneEnd = -1;
            
            // Попробуем найти номер телефона в кавычках
            if (statusEnd != -1) {
                phoneStart = header.indexOf("\"", statusEnd);
                if (phoneStart != -1) {
                    phoneStart += 1; // Пропускаем открывающую кавычку
                    phoneEnd = header.indexOf("\"", phoneStart);
                    if (phoneEnd != -1) {
                        phoneNumber = header.substring(phoneStart, phoneEnd);
                    }
                }
            }
            
            // Если номер не найден через кавычки, попробуем найти его в другом формате
            if (phoneNumber == "" || phoneNumber == ",") {
                // Ищем номер в формате без кавычек (например, "651")
                int afterStatus = statusEnd != -1 ? statusEnd + 1 : commaPos + 1;
                phoneStart = header.indexOf(",", afterStatus) + 1;
                if (phoneStart > 0 && phoneStart < header.length()) {
                    phoneEnd = header.indexOf(",", phoneStart);
                    if (phoneEnd == -1) phoneEnd = header.length();
                    
                    if (phoneEnd > phoneStart) {
                        phoneNumber = header.substring(phoneStart, phoneEnd);
                        phoneNumber.replace("\"", "");
                        phoneNumber.replace(",", "");
                        phoneNumber.trim();
                        
                        // Если номер все еще пустой или состоит из запятой, попробуем другой подход
                        if (phoneNumber == "" || phoneNumber == ",") {
                            // Ищем номер как последовательность цифр
                            for (int i = 0; i < header.length(); i++) {
                                if (isdigit(header[i])) {
                                    phoneStart = i;
                                    while (i < header.length() && isdigit(header[i])) i++;
                                    phoneEnd = i;
                                    phoneNumber = header.substring(phoneStart, phoneEnd);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            
            // Если номер все еще не найден, используем "Unknown"
            if (phoneNumber == "" || phoneNumber == ",") {
                phoneNumber = "Unknown";
            }
            
            // Извлечение временной метки
            String timestamp = "";

            // Ищем позицию 4-й запятой (перед временной меткой)
            int commaPos1 = header.indexOf(",");
            int commaPos2 = (commaPos1 != -1) ? header.indexOf(",", commaPos1 + 1) : -1;
            int commaPos3 = (commaPos2 != -1) ? header.indexOf(",", commaPos2 + 1) : -1;
            int commaPos4 = (commaPos3 != -1) ? header.indexOf(",", commaPos3 + 1) : -1;

            if (commaPos4 != -1 && commaPos4 < header.length() - 1) {
                // Проверяем, есть ли кавычка сразу после 4-й запятой
                int timestampStart = commaPos4 + 1;
                if (timestampStart < header.length() && header.charAt(timestampStart) == '"') {
                    timestampStart++; // Пропускаем открывающую кавычку
                }
                
                int timestampEnd = header.indexOf("\"", timestampStart);
                if (timestampEnd == -1) {
                    timestampEnd = header.length(); // Если закрывающей кавычки нет, берем до конца строки
                }
                
                timestamp = header.substring(timestampStart, timestampEnd);
                
                // Проверяем, что временная метка имеет правильный формат
                if (timestamp.length() >= 17) {
                    // ПРАВИЛЬНЫЙ ПОРЯДОК КОМПОНЕНТОВ: YY/MM/DD,HH:MM:SS+TZ
                    String year = "20" + timestamp.substring(0, 2); // Год (первые 2 цифры)
                    String month = timestamp.substring(3, 5);      // Месяц (позиции 3-4)
                    String day = timestamp.substring(6, 8);        // День (позиции 6-7)
                    String hour = timestamp.substring(9, 11);      // Часы (позиции 9-10)
                    String minute = timestamp.substring(12, 14);   // Минуты (позиции 12-13)
                    String second = timestamp.substring(15, 17);   // Секунды (позиции 15-16)
                    String tz = "";
                    
                    if (timestamp.length() > 17) {
                        tz = timestamp.substring(17);
                        // Убедимся, что часовой пояс в правильном формате
                        if (tz.length() > 0 && tz.charAt(0) != '+' && tz.charAt(0) != '-') {
                            tz = "+" + tz;
                        }
                    } else {
                        tz = "+00";
                    }
                    
                    // Формируем корректную временную метку в формате ISO 8601
                    timestamp = year + "-" + month + "-" + day + "T" + 
                              hour + ":" + minute + ":" + second + tz;
                }
            }

            // Если временная метка не найдена или имеет неверный формат, используем текущее время
            if (timestamp == "" || timestamp.length() < 17) {
                if (DEBUG_LOGS) {
                    Serial.println("Временная метка не распарсена, используем текущее время");
                }
                timestamp = getCurrentDateTime();
            }


            // Извлечение текста сообщения
            int msgStart = response.indexOf("\r\n", end) + 2;
            if (msgStart == 1) msgStart = response.length(); // Если нет переноса строки
            
            int msgEnd = response.indexOf("\r\n", msgStart);
            if (msgEnd == -1) msgEnd = response.length();
            
            String message = response.substring(msgStart, msgEnd);
            
            // Декодируем UCS2/UTF-16 сообщение, если оно в шестнадцатеричном формате
            if (message.length() >= 4) {
                bool isHex = true;
                for (int i = 0; i < 4; i++) {
                    char c = message.charAt(i);
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                        isHex = false;
                        break;
                    }
                }
                
                if (isHex) {
                    message = decodeUcs2(message);
                }
            }
            
            // Создаем объект сообщения
            SmsMessage sms;
            sms.phoneNumber = phoneNumber;
            sms.message = message;
            sms.timestamp = timestamp;
            sms.index = msgIndex;

            messages.push_back(sms);
            
            if (DEBUG_LOGS) {
                int displayLength = message.length() < 20 ? message.length() : 20;
                Serial.println("Найдено СМС от " + phoneNumber + ": " + message.substring(0, displayLength) + "...");
                Serial.println("Временная метка: " + timestamp);
            }
          
            index = msgEnd;
        }
    } else {
        if (DEBUG_LOGS) {
            Serial.println("В ответе нет данных о СМС. Полный ответ: " + response);
        }
    }
    
    return messages;
}

// Функция для декодирования UCS2/UTF-16 сообщений
String Sim800lClient::decodeUcs2(String hexString) {
    String result = "";
    
    // Убедимся, что строка имеет четную длину и не пустая
    if (hexString.length() < 4 || hexString.length() % 4 != 0) {
        return hexString; // Возвращаем как есть, если не можем декодировать
    }
    
    for (int i = 0; i < hexString.length(); i += 4) {
        // Извлекаем 4 символа (2 байта)
        String hexChar = hexString.substring(i, i + 4);
        
        // Преобразуем из шестнадцатеричной строки в целое число
        unsigned int charCode = (unsigned int)strtol(hexChar.c_str(), NULL, 16);
        
        // Конвертируем в UTF-8
        if (charCode < 0x80) {
            // 1 байт
            result += (char)charCode;
        } else if (charCode < 0x800) {
            // 2 байта
            result += (char)(0xC0 | (charCode >> 6));
            result += (char)(0x80 | (charCode & 0x3F));
        } else {
            // 3 байта
            result += (char)(0xE0 | (charCode >> 12));
            result += (char)(0x80 | ((charCode >> 6) & 0x3F));
            result += (char)(0x80 | (charCode & 0x3F));
        }
    }
    
    return result;
}


bool Sim800lClient::sendSmsToServer(std::vector<SmsMessage> messages, ApiParams* api) {
    if (messages.empty()) {
        Serial.println("sendSmsToServer messages.empty");
        return true; // Нечего отправлять
    }
    
    // Подготовка JSON данных (обрабатываем ВСЕ сообщения из полученного вектора)
    String jsonData = "[";
    for (int i = 0; i < messages.size(); i++) {
        if (i > 0) jsonData += ",";
        
        // Экранируем специальные символы в JSON
        String escapedPhone = escapeJson(messages[i].phoneNumber);
        String escapedMessage = escapeJson(messages[i].message);
        String escapedTimestamp = escapeJson(messages[i].timestamp);
        
        jsonData += "{\"phone\":\"" + escapedPhone + "\",";
        jsonData += "\"message\":\"" + escapedMessage + "\",";
        jsonData += "\"timestamp\":\"" + escapedTimestamp + "\"}";
    }
    jsonData += "]";
    
    if (DEBUG_LOGS) {
        int displayLength = jsonData.length() < 100 ? jsonData.length() : 100;
        Serial.println("Подготовленные данные для отправки СМС (длина: " + String(jsonData.length()) + "): " + jsonData.substring(0, displayLength) + "...");
    }
    
    // Установка URL
    String url = api->sendSmsUrl();
    String urlCommand = "AT+HTTPPARA=\"URL\",\"" + url + "\"";
    if (sendATcommand(urlCommand.c_str(), "OK", 10000).indexOf("OK") == -1) {
        if (DEBUG_LOGS) {
            Serial.println("Не удалось установить URL для отправки СМС");
        }
        return false;
    }
    
    // Установка заголовков
    sendATcommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 1000);
    
    // Отправка данных
    String dataCommand = "AT+HTTPDATA=" + String(jsonData.length()) + ",10000";
    String response = sendATcommand(dataCommand, "DOWNLOAD", 10000);
    
    if (response.indexOf("DOWNLOAD") != -1) {
        if (DEBUG_LOGS) {
            Serial.println("Входим в режим передачи данных...");
        }
        
        // Даем модулю время перейти в режим передачи
        delay(500);
        
        // Отправка JSON
        Serial2.print(jsonData);
        
        if (DEBUG_LOGS) {
            Serial.println("Данные отправлены, запускаем HTTPACTION...");
        }
        
        // Выполняем POST-запрос (1 = POST)
        String actionResponse = sendATcommand("AT+HTTPACTION=1", "+HTTPACTION:", 60000);
        
        if (actionResponse.indexOf("+HTTPACTION: 1,200") != -1) {
            if (DEBUG_LOGS) {
                Serial.println("Данные СМС успешно отправлены");
            }
            
            // УДАЛЕНИЕ SMS С SIM-КАРТЫ ПОСЛЕ УСПЕШНОЙ ОТПРАВКИ
            if (deviceSettings.deleteSmsAfterSend) {
                for (int i = 0; i < messages.size(); i++) {
                    int msgIndex = messages[i].index;
                    String deleteCommand = "AT+CMGD=" + String(msgIndex);
                    String deleteResponse = sendATcommand(deleteCommand, "OK", 5000);
                    
                    if (deleteResponse.indexOf("OK") != -1) {
                        if (DEBUG_LOGS) {
                            Serial.println("СМС #" + String(msgIndex) + " удалено с SIM-карты");
                        }
                    } else {
                        if (DEBUG_LOGS) {
                            Serial.println("Ошибка при удалении СМС #" + String(msgIndex));
                        }
                    }
                }
            }
            return true;
        } else {
            if (DEBUG_LOGS) {
                Serial.println("Ошибка отправки данных СМС: " + actionResponse);
            }
            // Попробуем прочитать возможный ответ
            String readResponse = sendATcommand("AT+HTTPREAD", "OK", 5000);
            if (DEBUG_LOGS && readResponse.length() > 0) {
                Serial.println("Содержимое ответа сервера: " + readResponse);
            }
            return false;
        }
    } else {
        if (DEBUG_LOGS) {
            Serial.println("Не удалось войти в режим загрузки данных");
        }
        return false;
    }
}

// Экранирование специальных символов в JSON
String Sim800lClient::escapeJson(String str) {
    String result = "";
    
    for (int i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        
        switch (c) {
            case '"': 
                result += "\\\""; 
                break;
            case '\\': 
                result += "\\\\"; 
                break;
            case '/': 
                result += "\\/"; 
                break;
            case '\b': 
                result += "\\b"; 
                break;
            case '\f': 
                result += "\\f"; 
                break;
            case '\n': 
                result += "\\n"; 
                break;
            case '\r': 
                result += "\\r"; 
                break;
            case '\t': 
                result += "\\t"; 
                break;
            default:
                // Для не-ASCII символов оставляем как есть (они уже в UTF-8)
                if (c < 0x20 || c > 0x7E) {
                    result += c;
                } else {
                    result += c;
                }
        }
    }
    
    return result;
}