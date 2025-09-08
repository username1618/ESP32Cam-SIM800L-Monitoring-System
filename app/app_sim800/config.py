import logging
from pydantic_settings import BaseSettings
from typing import Set, List, Dict
from logging.handlers import RotatingFileHandler

# Базовые значения этих параметров переопределяться через файл .env
class Settings(BaseSettings):
    app_port_api: int = 15000
    behind_proxy:bool = True

    # Пути и директории
    image_folder: str = 'static/images'
    csv_file_path: str = 'data/csv/signals.csv'
    csv_sms_file_path: str = 'data/csv/sms.csv'
    camera_config_path: str = 'camera_settings.json'
    blocked_ips_file: str = 'blocked_ips.txt'
    logs_path_api: str = 'api.log'
    
    # Настройки безопасности
    trusted_ips: Set[str] = {}
    image_extensions: Set[str] = {'jpg'}
    allowed_origins: List[str] = ["*"]
    max_upload_size: int = 5_000_000  # 5MB
    display_last_images: int = 1000
    
    # Причины пробуждения
    wakeup_reasons: Dict[str, str] = {
        '0': 'плановое',
        '4': 'плановое',
        '2': 'датчик движения',
        '7': 'GPIO'
    }
    
    #TODO: remove (это переменные для web-ui)
    app_port_web: int = 15001
    logs_path_web: str = ''
    redis_password: str = ''

    
    class Config:
        env_file = ".env"
        env_file_encoding = 'utf-8'


def setup_logging(config: Settings):
    """Настройка системы логирования"""
    logger = logging.getLogger()
    logger.setLevel(logging.INFO)

    formatter = logging.Formatter(
        '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    )

    # Ротирующий файловый обработчик
    file_handler = RotatingFileHandler(
        config.logs_path_api,
        maxBytes=10 * 1024 * 1024,  # 10 MB
        backupCount=5,
        encoding='utf-8'
    )
    file_handler.setFormatter(formatter)

    # Консольный вывод
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(formatter)

    logger.addHandler(file_handler)
    logger.addHandler(console_handler)