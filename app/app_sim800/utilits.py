import json
import logging
import asyncio
import aiofiles

from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from pathlib import Path
from typing import Tuple, List, Dict
from io import StringIO

import pandas as pd
from fastapi import UploadFile, HTTPException
from werkzeug.utils import secure_filename

from config import Settings
from security import SecurityPatterns


config = Settings()
executor = ThreadPoolExecutor()


async def run_in_thread(func, *args, **kwargs):
    """Асинхронный запуск синхронной функции в потоке"""
    return await asyncio.get_event_loop().run_in_executor(
        executor, lambda: func(*args, **kwargs)
    )


def validate_filename(filename: str, config: Settings) -> bool:
    """Валидация имени файла"""
    return (
        '.' in filename and 
        filename.rsplit('.', 1)[1].lower() in config.image_extensions and
        SecurityPatterns.TRAVERSAL.search(filename) is None
    )
        
async def process_upload(file: UploadFile, config: Settings) -> Dict:
    """Основной процесс обработки загрузки файла"""
    try:
        file_path = await save_file(file, config)
        await update_stats(file_path, config)
        await clean_directory(config)
        return {"filename": file.filename}
    except Exception as e:
        logging.error(f"Upload processing failed: {str(e)}", exc_info=True)
        raise HTTPException(500, "File processing error")

async def save_file(file: UploadFile, config: Settings) -> Path:
    """Сохранение файла с генерацией безопасного имени"""
    upload_dir = Path(config.image_folder)
    upload_dir.mkdir(exist_ok=True)
    
    safe_name = secure_filename(file.filename)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    file_path = upload_dir / f"{timestamp}_{safe_name}"
    
    async with aiofiles.open(file_path, "wb") as buffer:
        while content := await file.read(1024 * 1024):  # 1MB chunks
            await buffer.write(content)
    
    return file_path



def extract_info_from_filename(filename: str) -> Tuple:
    """Извлечение метаданных из имени файла"""
    try:
        base_name = Path(filename).stem
        parts = base_name.split('_')

        # Парсинг даты и времени
        date_part = parts[0]
        time_part = parts[1]
        date_str = f"{date_part[6:8]}.{date_part[4:6]}.{date_part[0:4]}"
        time_str = f"{time_part[0:2]}:{time_part[2:4]}:{time_part[4:6]}"
        
        # Извлечение остальных параметров
        return (
            int(parts[4]) / 1000 if len(parts) > 4 else 'N/A',  # voltage_sim
            float(parts[14]) if len(parts) > 14 else 'N/A',     # voltage_akb
            int(int(parts[6].replace(',', '')) / 31 * 100) if len(parts) > 6 else 'N/A',  # signal_level
            config.wakeup_reasons.get(parts[8], 'N/A') if len(parts) > 8 else 'N/A',  # reason
            f"{date_str} {time_str}",  # datetime_str
            float(parts[12]) if len(parts) > 12 else 'N/A',  # humidity
            float(parts[10]) if len(parts) > 10 else 'N/A'   # temperature
        )
    except (IndexError, ValueError, TypeError) as e:
        logging.warning(f"Error parsing filename {filename}: {str(e)}")
        return ('N/A',) * 7

async def update_stats(file_path: Path, config: Settings) -> None:
    """Обновление статистики в CSV файле"""
    try:
        data = await parse_file_data(file_path.name, config)
        await write_to_csv(data, config.csv_file_path)
    except Exception as e:
        logging.error(f"Failed to update stats: {str(e)}", exc_info=True)
        raise

async def parse_file_data(filename: str, config: Settings) -> pd.DataFrame:
    """Парсинг данных из имени файла"""
    file_info = await run_in_thread(extract_info_from_filename, filename)
    return pd.DataFrame({
        'date': [file_info[4]],
        'humidity': [file_info[5]],
        'temperature': [file_info[6]],
        'signal': [file_info[2]],
        'voltage_sim': [file_info[0]],
        'voltage_akb': [file_info[1]]
    })

async def write_to_csv(new_df: pd.DataFrame, csv_path: str) -> None:
    """Асинхронная запись данных в CSV"""
    path = Path(csv_path)
    
    # Чтение существующих данных
    existing_df = await read_existing_csv(path) if await run_in_thread(path.exists) else pd.DataFrame()

    # Объединение и дедупликация
    combined_df = await run_in_thread(
        lambda: pd.concat([existing_df, new_df])
                  .drop_duplicates(subset='date', keep='last')
    )

    # Асинхронная запись
    async with aiofiles.open(path, 'w') as f:
        csv_content = await run_in_thread(
            combined_df.to_csv, 
            index=False, 
            lineterminator='\n'
        )
        await f.write(csv_content)

async def read_existing_csv(path: Path) -> pd.DataFrame:
    """Чтение существующего CSV файла"""
    try:
        async with aiofiles.open(path, 'r') as f:
            content = await f.read()
            return await run_in_thread(pd.read_csv, StringIO(content))
    except Exception as e:
        logging.warning(f"Error reading CSV {path}: {str(e)}")
        return pd.DataFrame()

async def clean_directory(config: Settings) -> List[str]:
    """Очистка директории от старых файлов"""
    try:
        image_dir = Path(config.image_folder)
        if not await run_in_thread(image_dir.exists):
            return []

        # Получение и сортировка файлов
        image_files = await get_sorted_images(image_dir)
        
        # Удаление старых файлов
        await delete_old_files(image_files, config.display_last_images)
        
        return [str(f) for f in image_files[:config.display_last_images]]
    except Exception as e:
        logging.error(f"Directory cleanup failed: {str(e)}", exc_info=True)
        raise

async def get_sorted_images(image_dir: Path) -> List[Path]:
    """Получение отсортированного списка изображений"""
    return await run_in_thread(
        lambda: sorted(
            image_dir.glob('*.jpg'),
            key=lambda x: x.stat().st_mtime,
            reverse=True
        )
    )

async def delete_old_files(files: List[Path], keep_count: int) -> None:
    """Удаление файлов сверх лимита"""
    for file in files[keep_count:]:
        try:
            await run_in_thread(file.unlink)
        except Exception as e:
            logging.warning(f"Error deleting {file}: {str(e)}")

async def load_cached_settings(config: Settings) -> Dict:
    """Загрузка настроек из JSON файла"""
    config_file = Path(config.camera_config_path)

    if not await run_in_thread(config_file.exists):
        raise FileNotFoundError(f"Config file {config_file} not found")

    async with aiofiles.open(config_file, 'r', encoding='utf-8') as f:
        content = await f.read()
        return await run_in_thread(json.loads, content)

def flatten_settings(data: Dict, prefix: str = '') -> Dict:
    """Рекурсивное преобразование структуры настроек"""
    result = {}
    for key, value in data.items():
        current_key = f"{prefix}{key}"
        if isinstance(value, dict):
            if 'current' in value:
                result[current_key] = value['current']
            else:
                result.update(flatten_settings(value, f"{current_key}_"))
    return result