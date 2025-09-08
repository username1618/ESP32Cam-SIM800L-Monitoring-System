// Настройки API сервера
struct ApiParams{
    bool useHttps = false;
    String serverAddress = "";

    String uploadEndpoint = "/upload";
    String settingsEndpoint = "/settings-flat";
    String smsEndpoint = "/sms-receive";

    String baseUrl() const {
        return (useHttps ? "https://" : "http://") + serverAddress;
    }
    String getSettingsUrl() const {
        return baseUrl() + settingsEndpoint;
    }
    String uploadUrl() const {
        return baseUrl() + uploadEndpoint;
    }
    String sendSmsUrl() const {
        return baseUrl() + smsEndpoint;
    }

};