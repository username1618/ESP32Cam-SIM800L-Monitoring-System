import re
import logging
import asyncio
import aiofiles

from pathlib import Path
from fastapi import Request
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.types import ASGIApp

from config import Settings
from limiter import get_client_ip

class SecurityPatterns:
    # Компилируем все паттерны в регулярные выражения
    BLOCKED_PATHS = re.compile(
        r'(\.env|\.git|config\.json|secrets\.ya?ml|php-cgi|wp-admin|adminer|backup|'
        r'\.powenv|\.profile|cgi-bin|bin/sh|eval-stdin\.php|pearcmd|\.s3cfg|vendor/phpunit|'
        r'think/app|Util/PHP|_info\.php|_phpinfo\.php|restore\.php|backup\.php|recovery\.php|'
        r'_poopinfo\.php|wp-login\.php|admin\.php|config\.php|\.phpinfo|_profiler/info)',
        re.IGNORECASE
    )
    
    BLOCKED_KEYWORDS = re.compile(
        r'(allow_url_include|auto_prepend_file|call_user_func_array|invokefunction|'
        r'config-create|/../../|%2e%2e|xdebug_session_start|phpstorm|debug)',
        re.IGNORECASE
    )
    
    TRAVERSAL = re.compile(
        r'(\.\./|\.\.\\|%2e%2e/|%2e%2e%2f|\.\.;/|\.\.5c|\.\.%255c|\.\.%c0%af)',
        re.IGNORECASE
    )
    
    PHP_INJECTION = re.compile(
        r'(php://|<\?php|<\?=|phar://|expect://|zlib://|data://|glob://|ssh2://)',
        re.IGNORECASE
    )
    
    EXPLOITS = re.compile(
        r'(s=/index/\\think\\app/invokefunction|function=call_user_func_array|'
        r'vars\[0\]=md5|lang=../../../../../../../../)',
        re.IGNORECASE
    )
    
    USER_AGENTS = re.compile(
        r'(nmap|sqlmap|nikto|metasploit|havij|dirbuster|wpscan|acunetix|nessus|'
        r'firefox/14\.0a1|firefox/44\.0|chrome/3\.0\.195\.17)',
        re.IGNORECASE
    )

               
class IPBlocker:
    """Менеджер блокировки IP-адресов"""
    def __init__(self, config: Settings):
        self.config = config
        self._blocked_ips = set()
        self._last_modified = 0.0
        self._lock = asyncio.Lock()

    async def update_cache(self):
        """Обновление кэша заблокированных IP"""
        try:
            path = Path(self.config.blocked_ips_file)
            current_mtime = path.stat().st_mtime
            
            if current_mtime > self._last_modified:
                async with self._lock:
                    async with aiofiles.open(path, 'r', encoding='utf-8') as f:
                        ips = await f.readlines()
                        self._blocked_ips = {ip.strip() for ip in ips if ip.strip()}
                    self._last_modified = current_mtime
        except Exception as e:
            logging.error(f"Ошибка обновления кэша IP: {e}")

    async def is_blocked(self, ip: str) -> bool:
        """Проверка блокировки IP"""
        await self.update_cache()
        return ip in self._blocked_ips

    async def block_ip(self, ip: str, reason: str):
        """Блокировка нового IP"""
        if ip in self.config.trusted_ips:
            return

        async with self._lock:
            try:
                async with aiofiles.open(self.config.blocked_ips_file, 'a', encoding='utf-8') as f:
                    await f.write(f"{ip}\n")
                self._blocked_ips.add(ip)
                logging.warning(f"Заблокирован IP: {ip} - Причина: {reason}")
            except Exception as e:
                logging.error(f"Ошибка блокировки IP {ip}: {e}")                
            
            
class SecurityMiddleware(BaseHTTPMiddleware):
    """Middleware для обработки безопасности"""
    def __init__(self, app: ASGIApp, config: Settings):
        super().__init__(app)
        self.config = config
        self.ip_blocker = IPBlocker(config)
        self.security_patterns = SecurityPatterns()

    async def dispatch(self, request: Request, call_next):
        #client_ip = request.client.host if request.client else "unknown"
        client_ip = get_client_ip(request)
        
        # Пропускаем доверенные IP
        if client_ip in self.config.trusted_ips:
            return await call_next(request)
        
        # Проверка заблокированных IP
        if await self.ip_blocker.is_blocked(client_ip):
            logging.warning(f"Попытка доступа с заблокированного IP: {client_ip}")
            return JSONResponse(
                {"error": "Доступ запрещен"},
                status_code=403
            )

        # Проверка безопасности запроса
        if await self.check_security_violations(request, client_ip):
            return JSONResponse(
                {"error": "Обнаружена угроза безопасности"},
                status_code=403
            )

        # Обработка запроса
        try:
            return await call_next(request)
        except Exception as e:
            logging.error(f"Ошибка обработки запроса: {e}", exc_info=True)
            await self.ip_blocker.block_ip(client_ip, f"Ошибка сервера: {str(e)}")
            return JSONResponse(
                {"error": "Внутренняя ошибка сервера"},
                status_code=500
            )

    async def check_security_violations(self, request: Request, client_ip: str) -> bool:
        """Выполнение проверок безопасности"""
        url = str(request.url).lower()
        user_agent = request.headers.get("User-Agent", "").lower()
        full_check = f"{request.url.path}?{request.url.query}".lower()

        checks = (
            (self.security_patterns.BLOCKED_PATHS, "Обнаружен опасный путь"),
            (self.security_patterns.BLOCKED_KEYWORDS, "Обнаружено опасное ключевое слово"),
            (self.security_patterns.TRAVERSAL, "Попытка обхода директорий"),
            (self.security_patterns.PHP_INJECTION, "Попытка PHP-инъекции"),
            (self.security_patterns.EXPLOITS, "Обнаружен известный эксплойт"),
            (self.security_patterns.USER_AGENTS, "Обнаружен опасный User-Agent"),
        )

        for pattern, reason in checks:
            if pattern.search(full_check) or (pattern == self.security_patterns.USER_AGENTS and pattern.search(user_agent)):
                await self.ip_blocker.block_ip(client_ip, f"{reason}: {pattern.pattern}")
                return True

        if len(url) > 512:
            await self.ip_blocker.block_ip(client_ip, "Слишком длинный URL")
            return True

        return False


def setup_cors(app: ASGIApp, config: Settings) -> None:
    """Настройка CORS"""
    app.add_middleware(
        CORSMiddleware,
        allow_origins=config.allowed_origins,
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )
                

