from fastapi import FastAPI, Request, status
from fastapi.responses import JSONResponse
from slowapi.errors import RateLimitExceeded

from limiter import limiter
from config import Settings, setup_logging
from endpoints.upload import upload_router
from endpoints.settings import settings_router
from endpoints.sms import router as sms_router
from security import SecurityMiddleware, setup_cors


# --- Инициализация конфигурации ---
config = Settings()

# --- Логирование ---
setup_logging(config)

# --- Инициализация приложения ---
app = FastAPI(title="Camera API", version="0.0.1")

# --- Подключение роутеров --- 
app.include_router(upload_router)
app.include_router(settings_router)
app.include_router(sms_router)

# --- Ограничение запросов ---
app.state.limiter = limiter

@app.exception_handler(RateLimitExceeded)
async def rate_limit_handler(request: Request, exc: RateLimitExceeded):
    return JSONResponse(
        {"error": "Too many requests"},
        status_code=status.HTTP_429_TOO_MANY_REQUESTS
    )

# --- Безопасность ---

# Настройка CORS
setup_cors(app, config)

# Добавление middleware безопасности
app.add_middleware(SecurityMiddleware, config=config)


# --- Запуск приложения ---
if __name__ == "__main__":
    import uvicorn
    uvicorn.run(
        app,
        host="0.0.0.0",
        port=config.app_port_api,
        #ssl_keyfile="key.pem",
        #ssl_certfile="cert.pem"
    )




