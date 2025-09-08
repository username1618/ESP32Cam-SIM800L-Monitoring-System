import json
import logging

from fastapi import APIRouter, HTTPException, Request
from fastapi.responses import JSONResponse

from config import Settings
from utilits import load_cached_settings, flatten_settings
from limiter import limiter


config = Settings()
settings_router = APIRouter()

@settings_router.get("/settings-flat")
@limiter.limit("50/day")
@limiter.limit("5/hour")
async def get_settings_flat(request: Request):
    """ Получение плоских настроек. Вызывается в ESP32
    """
    print("Запрос принят! settings-flat", request.headers.get("X-Forwarded-For"))
    try:
        settings = await load_cached_settings(config)
        return JSONResponse(
            flatten_settings(settings),
            headers={"Cache-Control": "public, max-age=300"}
        )
    except FileNotFoundError:
        raise HTTPException(404, detail="Файл настроек не найден")
    except json.JSONDecodeError:
        raise HTTPException(500, detail="Ошибка формата настроек")
    except Exception as e:
        logging.error(f"Ошибка получения настроек: {str(e)}", exc_info=True)
        raise HTTPException(500, detail="Внутренняя ошибка сервера")
    


def load_settings():
    try:
        with open(config.camera_config_path, 'r') as f:
            return json.load(f)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Error loading settings: {str(e)}")

def save_settings(settings):
    try:
        with open(config.camera_config_path, 'w') as f:
            json.dump(settings, f, indent=2)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Error saving settings: {str(e)}")


@settings_router.get("/handle_settings")
async def get_settings():
    print("Запрос принят! get settings")
    try:
        settings = load_settings()
        return settings  # FastAPI автоматически преобразует словарь в JSON
    except Exception as e:
        return JSONResponse(
            {"error": "Error loading settings", "details": str(e)},
            status_code=500
        )

@settings_router.post("/handle_settings")
async def update_settings(request: Request):
    """ Сохранение измененных настроек
    """
    content_type = request.headers.get("content-type", "")
    if "application/json" not in content_type.lower():
        return JSONResponse(
            {"success": False, "error": "Invalid content type. Expected application/json"},
            status_code=400
        )
    
    try:
        print("Запрос принят! set settings")

        # Загружаем текущие настройки
        updates = await request.json()
        current_settings = load_settings()
        
        # Функция для рекурсивного обновления настроек
        def apply_updates(base, updates, path=None):
            if path is None:
                path = []
                
            for key, value in updates.items():
                current_path = path + [key]
                if key in base:
                    if isinstance(value, dict):
                        apply_updates(base[key], value, current_path)
                    elif isinstance(base[key], dict) and 'current' in base[key]:
                        # Сохраняем тип данных
                        current_value = base[key]['current']
                        if isinstance(current_value, bool):
                            base[key]['current'] = bool(value)
                        elif isinstance(current_value, int):
                            base[key]['current'] = int(value)
                        elif isinstance(current_value, float):
                            base[key]['current'] = float(value)
                        else:
                            base[key]['current'] = value
                    else:
                        print(f"Warning: Setting {'/'.join(current_path)} not found or invalid")
        
        # Обновляем настройки
        apply_updates(current_settings, updates)
        
        # Сохраняем обновленные настройки
        save_settings(current_settings)
        
        return JSONResponse({
            "success": True,
            "settings": current_settings,
            "message": "Settings saved successfully"
        })
    except Exception as e:
        print(f"Error in save_settings: {str(e)}")
        return JSONResponse(
            {"success": False, "error": str(e)},
            status_code=500
        )






# @router.get("/blocked_ips")
# async def get_blocked_ips():
#     try:
#         with open(config.blocked_ips_file, 'r') as f:
#             ips = [line.strip() for line in f if line.strip()]
#         return {"blocked_ips": ips}
#     except Exception as e:
#         raise HTTPException(status_code=500, detail=f"Error reading blocked IPs: {str(e)}")

# @router.post("/blocked_ips")
# async def add_blocked_ip(request: Request):
#     data = await request.json()
#     ip = data.get("ip")
#     if not ip:
#         raise HTTPException(status_code=400, detail="IP address is required")
    
#     try:
#         with open(config.blocked_ips_file, 'a') as f:
#             f.write(f"{ip}\n")
#         return {"status": "success"}
#     except Exception as e:
#         raise HTTPException(status_code=500, detail=f"Error adding blocked IP: {str(e)}")