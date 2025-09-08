from slowapi import Limiter
from slowapi.util import get_remote_address
from fastapi import Request
from config import Settings


config = Settings()

def get_client_ip(request: Request) -> str:
    """Определение IP клиента с учетом прокси"""
    if config.behind_proxy:
        return request.headers.get("X-Forwarded-For", get_remote_address(request))
    return get_remote_address(request)

def rate_limit_key_func(request: Request) -> str:
    """Генерация ключа для лимитера с исключением доверенных IP"""
    client_ip = get_client_ip(request)
    
    if client_ip in config.trusted_ips:
        return None
        
    return client_ip

# Инициализация лимитера с кастомной функцией
limiter = Limiter(
    key_func=rate_limit_key_func,
    default_limits=["100/day", "5/hour"],
    headers_enabled=True
)