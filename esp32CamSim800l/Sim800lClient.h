#include <Arduino.h>
#include "esp_camera.h"

#include "apnParams.h"
#include "apiParams.h"
#include "deviceSettings.h"

#define DEBUG_LOGS false // логирование через Serial (отключить в проде для экономии энергии)

// Структура для хранения СМС
struct SmsMessage {
    String phoneNumber;
    String message;
    String timestamp;
    int index;
};


class Sim800lClient
{
    private:
      String ftpServerAddress;
      int ftpServerPort;
      String ftpUser;
      String ftpPassword;
      boolean sendDataPage(uint8_t *fbBuf, size_t bufLength);
      String readLineFromSerial(String stringToRead, unsigned long timeoutMillis);
      String sendATcommand(String toSend, String expectedResponse, unsigned long milliseconds); 
      void blinkRed(int count, int onTime, int offTime);
      int parseStatusCode(String response);
    public:
      Sim800lClient(void);
      void resetGsm(int resetPinNum);
      boolean waitForGsmNetwork();
      boolean goToSleep();
      boolean initFtp(String _ftpServerAddress, int _ftpServerPort, String _ftpUser, String _ftpPassword);
      void stopFtp(void);
      boolean sendFileToFtp(camera_fb_t * fb, String remoteFileName);
      boolean startHttp(ApiParams* api, APNSettings* apn);
      void stopHttp(void);
      bool getCameraSettingsHttp(ApiParams* api, DeviceSettings* settings);
      boolean sendFileByHttp(camera_fb_t * fb, String fileName, ApiParams* api);
      void sendSMS(String number, String message);
      void makeCall(String number);
      void waitForCallToEnd();
      String getBatteryVoltage(void);
      String getSignalStrength(void);
      String getNetworkOperator(void);
      String getCurrentDateTime(void);

      std::vector<SmsMessage> getNewSms();
      String decodeUcs2(String hexString);
      bool sendSmsToServer(std::vector<SmsMessage> messages, ApiParams* api);
      String escapeJson(String str);
};