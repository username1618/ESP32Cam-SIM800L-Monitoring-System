import csv
import os
from typing import List
from datetime import datetime
from pydantic import BaseModel
from fastapi import APIRouter, HTTPException



from config import Settings

router = APIRouter()
config = Settings()

# Путь к CSV файлу для хранения СМС
SMS_CSV_PATH = config.csv_sms_file_path

# Убедимся, что директория существует
os.makedirs(os.path.dirname(SMS_CSV_PATH), exist_ok=True)

# Создаем CSV файл, если его нет
if not os.path.exists(SMS_CSV_PATH):
    with open(SMS_CSV_PATH, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['timestamp', 'phone', 'message'])

class SmsItem(BaseModel):
    phone: str
    message: str
    timestamp: str

@router.post("/sms-receive")
async def receive_sms(sms_items: List[SmsItem]):
    try:
        # Создаем временный список для всех записей
        all_records = []
        
        # Читаем существующие данные из CSV, если файл существует
        if os.path.exists(SMS_CSV_PATH):
            with open(SMS_CSV_PATH, 'r', newline='', encoding='utf-8') as f:
                reader = csv.reader(f)
                for row in reader:
                    if len(row) >= 3:  # Проверяем, что строка содержит все необходимые поля
                        all_records.append({
                            'timestamp': row[0],
                            'phone': row[1],
                            'message': row[2],
                            'original_row': row  # Сохраняем оригинальную строку для перезаписи
                        })
        
        # Добавляем новые записи
        for item in sms_items:
            all_records.append({
                'timestamp': item.timestamp,
                'phone': item.phone,
                'message': item.message,
                'original_row': [item.timestamp, item.phone, item.message]
            })
        
        # Удаляем дубликаты: считаем дубликатами записи с одинаковым номером, сообщением и временной меткой
        seen = set()
        unique_records = []
        for record in all_records:
            # Создаем уникальный ключ на основе всех трех полей
            record_key = (record['timestamp'], record['phone'], record['message'])
            if record_key not in seen:
                seen.add(record_key)
                unique_records.append(record)
        
        # Сортируем записи по временной метке
        # Пытаемся преобразовать временную метку в объект datetime для корректной сортировки
        def parse_timestamp(record):
            try:
                # Пытаемся распарсить временную метку в datetime объект
                # Здесь можно добавить более сложную логику парсинга в зависимости от формата временных меток
                return datetime.strptime(record['timestamp'], "%Y-%m-%dT%H:%M:%S%z")
            except:
                # Если парсинг не удался, возвращаем минимально возможную дату
                return datetime.min
        
        sorted_records = sorted(unique_records, key=parse_timestamp)
        
        # Перезаписываем CSV с уникальными и отсортированными записями
        with open(SMS_CSV_PATH, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            for record in sorted_records:
                writer.writerow(record['original_row'])
        
        return {"status": "success", "received": len(sms_items), "total_records": len(sorted_records)}
    except Exception as e:
        return HTTPException(status_code=500, detail=f"Error saving SMS: {str(e)}")


@router.get("/sms-last")
async def get_last_sms(limit: int = 5):
    try:
        sms_list = []
        if os.path.exists(SMS_CSV_PATH):
            with open(SMS_CSV_PATH, 'r', newline='', encoding='utf-8') as f:
                reader = csv.reader(f)
                headers = next(reader)  # Пропускаем заголовки
                sms_list = [row for row in reader]
                sms_list = sms_list[-limit:] if len(sms_list) > limit else sms_list
        
        # Форматируем результат
        result = []
        for sms in sms_list:
            result.append({
                "timestamp": sms[0],
                "phone": sms[1],
                "message": sms[2]
            })
        
        return {"sms": result}
    except Exception as e:
        return HTTPException(status_code=500, detail=f"Error reading SMS: {str(e)}")