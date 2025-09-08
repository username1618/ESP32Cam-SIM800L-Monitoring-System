import os
from datetime import timedelta


USE_NGINX = os.environ.get('USE_NGINX', False)
USE_HTTPS = os.environ.get('USE_HTTPS', True)
USE_REDIS = os.environ.get('USE_REDIS', False)

class ConfigApp:
    ENV = 'production'
    DEBUG = False
    SECRET_KEY = os.environ.get('SECRET_KEY') or 'your-secret-key-here'
    
    USE_NGINX = USE_NGINX
    USE_HTTPS = USE_HTTPS
    USE_REDIS = USE_REDIS
    
    APP_PORT = 15001 if USE_NGINX else 15000

    # Security
    TRUSTED_IPS = {}  # 176.59.X.X - Tele2
    TRUSTED_PROXIES = ['172.21.0.0/16', 'nginx'] if USE_NGINX else []
    PREFERRED_URL_SCHEME = 'https' if USE_HTTPS else 'http'
        
    #USE_X_SENDFILE = False  # Важно для работы с файлами
               
    # Использовать HTTPS
    RUN_WITH_SSL = not USE_NGINX and USE_HTTPS
    CERT_FILE = 'ssl/fullchain.pem'
    KEY_FILE = 'ssl/privkey.pem'

    # Talisman (защитные механизмы)
    TALISMAN_ENABLED = True
    TALISMAN_CONFIG = {
        'force_https': USE_HTTPS,
        'strict_transport_security': USE_HTTPS,
        'session_cookie_secure': USE_HTTPS,
        'content_security_policy': {
            'default-src': "'self'",
            'img-src': "'self' data:",
            'script-src': "'self' 'unsafe-inline'",  # 'unsafe-inline' не рекомендуется для production Замените на nonce
            'style-src': "'self' 'unsafe-inline'"
        }
    }
    
    # Ограничение частоты запросов (кроме доверенных IP)
    LIMITER_ENABLED = True
    LIMITER_STORAGE_URI = os.getenv('REDIS_URL') if USE_REDIS else "memory://"
    LIMITER_STORAGE_OPTIONS = {
        "socket_connect_timeout": 30
    } if USE_REDIS else {
        "cleanup_interval": timedelta(minutes=3).total_seconds(),
    }
    
    # Cookies
    SESSION_COOKIE_SECURE = USE_HTTPS  # False for HTTP; True for HTTPS
    SESSION_COOKIE_HTTPONLY = True     # Защита от XSS (запрет обращения к кукам через JS)
    SESSION_COOKIE_SAMESITE = 'Lax'    # Защита от CSRF  'Secure' for HTTPS
    #PERMANENT_SESSION_LIFETIME = timedelta(days=1)
    
    # Макс. размер загружаемого файла
    MAX_CONTENT_LENGTH = int(0.5 * 1024 * 1024) # 0.5MB   


class ConfigMain:    

    # Глубина фотоархива (кол-во кадров)
    DISPLAY_N_LAST_IMAGES = 1000

    # Максимальная задержка между кадрами для "поднятия тревоги" (в минутах)
    CAMERA_UPDATE_MINUTES = 30 + 3

    # Путь к папке для сохранения изображений в методе /upload 
    IMAGE_FOLDER = 'static/images'
    
    # Разрешенные форматы присылаемых кадров в методе /upload 
    IMAGE_EXTENSIONS = {'jpg'} # 'jpg', 'jpeg', 'png', 'bmp'

    # Путь к csv файлу для сбора статистики
    CSV_FILE_PATH = 'data/csv/signals.csv'

    # Путь к конфигу с настрйоками устройства
    CAMERA_CONFIG_PATH = 'camera_settings.json'
    
    # Путь к файлу с заблокированными ip
    BLOCKED_IPS_FILE_PATH = 'blocked_ips.txt'
    
    # Путь к файлу для сохранения логов
    FLASK_LOGS_FILE_PATH = 'web.log'
    
    #
    WAKEUP_REASONS = {
        '0':'плановое',
        '4':'плановое',
        '2':'датчик движения',
        '7':'GPIO',
    }
    