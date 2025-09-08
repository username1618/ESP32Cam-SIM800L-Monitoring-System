import os
import glob
import json
import numpy as np
import pandas as pd
from PIL import Image, ImageEnhance



def load_json(filepath):
    """Загрузка JSON из файла"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError) as e:
        print(f"Error loading {filepath}: {str(e)}")
        raise

def save_json(filepath, data):
    """Сохранение данных в JSON файл"""
    with open(filepath, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=4)


def get_images(keep_count=None):
    """
    Получает список имен изображений из static папки, сортируя их по времени модификации.

    :param keep_count: Количество изображений, которые нужно вернуть. Если None, возвращаются все изображения.
    :return: Список имен изображений.
    """
    image_files = glob.glob(os.path.join(ConfigMain.IMAGE_FOLDER, '*.jpg')) 
    image_files.sort(key=os.path.getmtime, reverse=False)
    image_files = image_files[-keep_count:] if keep_count is not None else image_files
    image_names = [os.path.basename(file) for file in image_files]
    
    return image_names

def clean_directory(keep_count):
    """
    Очищает директорию, оставляя только указанное количество последних изображений.

    :param keep_count: Количество изображений, которые нужно оставить.
    :return: Список оставшихся изображений.
    """
    image_files = glob.glob(os.path.join(ConfigMain.IMAGE_FOLDER, '*.jpg')) 
    image_files.sort(key=os.path.getmtime, reverse=True)
    latest_images = image_files[:keep_count]

    # Удаление остальных изображений
    for image in image_files[keep_count:]:
        try:
            os.remove(image)
        except Exception as e:
            print(f"Error deleting file {image}: {e}")

    return latest_images

def extract_info_from_filename(filename):
    """
    Извлекает информацию из имени файла изображения.
    
    Формат входных данных:
           "DATE_TIME"
           String(USER_ID) + 
           "volt_" + String(BatteryVoltage) +
           "_sig_" + String(SignalStrength) +
           "_t_" + String(temperature) +
           "_h_" + String(humidity) +
           "_akb_" + String(akbVoltage) +
           ".jpg";

    :param filename: Имя файла изображения.
    :return: Кортеж с извлеченной информацией: 
             (симулированное напряжение, напряжение аккумулятора, уровень сигнала, причина, дата и время, высота, температура).
    """
    base_name = os.path.splitext(filename)[0]
    parts = base_name.split('_')

    date_str = parts[0]
    time_str = parts[1]
    formatted_date = f"{date_str[6:8]}.{date_str[4:6]}.{date_str[0:4]}"
    formatted_time = f"{time_str[0:2]}:{time_str[2:4]}:{time_str[4:6]}"
    datetime_str = f"{formatted_date} {formatted_time}"
    
    user_id = str(parts[2])

    try:
        voltage_sim = int(parts[4])/1000
    except:
        voltage_sim = 'N/A'
        
    try:
        signal_level = int(int(parts[6].replace(',',''))/31*100)
    except:
        signal_level = 'N/A'

    try:
        reason = parts[8]
        reason = ConfigMain.WAKEUP_REASONS.get(reason, 'N/A')
    except:
        reason = 'N/A'

    try:
        t = float(parts[10])
    except:
        t = 'N/A'

    try:
        h = float(parts[12])
    except:
        h = 'N/A'

    try:
        voltage_akb = float(parts[14])
    except:
        voltage_akb = 'N/A'
        
    # return {
        # 'voltage_sim': voltage_sim,
        # 'voltage_akb': voltage_akb,
        # 'signal_level': signal_level,
        # 'reason': reason,
        # 'date': datetime_str,
        # 'hue': h,
        # 'temp': t
    # }
    return voltage_sim, voltage_akb, signal_level, reason, datetime_str, h, t


def auto_adjust_brightness_contrast(image_path, output_path):
    # Open the image
    image = Image.open(image_path)
    
    # Convert image to numpy array for analysis
    image_array = np.array(image)

    # Calculate the mean brightness of the image
    mean_brightness = np.mean(image_array)

    # Define target brightness (you can adjust this value)
    target_brightness = 48  # Midpoint of 0-255 range

    # Calculate brightness adjustment factor
    brightness_factor = target_brightness / mean_brightness

    # Adjust brightness
    enhancer = ImageEnhance.Brightness(image)
    adjusted_image = enhancer.enhance(brightness_factor)

    # Calculate contrast adjustment
    contrast_enhancer = ImageEnhance.Contrast(adjusted_image)
    contrast_factor = 1.1  # You can adjust this value for more or less contrast
    adjusted_image = contrast_enhancer.enhance(contrast_factor)

    # Save the adjusted image
    adjusted_image.save(output_path)

def auto_adjust_color_balance(image_path, output_path):
    # Open the image
    image = Image.open(image_path)
    
    # Convert image to numpy array
    image_array = np.array(image)

    # Calculate the mean of each color channel (R, G, B)
    mean_r = np.mean(image_array[:, :, 0])  # Red channel
    mean_g = np.mean(image_array[:, :, 1])  # Green channel
    mean_b = np.mean(image_array[:, :, 2])  # Blue channel

    # Calculate the average mean
    mean_rgb = (mean_r + mean_g + mean_b) / 3

    # Calculate adjustment factors for each channel
    r_adjust = mean_rgb / mean_r
    g_adjust = mean_rgb / mean_g
    b_adjust = mean_rgb / mean_b

    # If the green channel is significantly higher, apply a stronger adjustment
    if mean_g > mean_rgb * 1.1:  # Adjust this threshold as needed
        g_adjust *= 2.0  # Reduce the green channel more aggressively

    # Apply adjustments to each channel
    adjusted_image_array = image_array.copy()
    adjusted_image_array[:, :, 0] = np.clip(adjusted_image_array[:, :, 0] * r_adjust, 0, 255)  # Red
    adjusted_image_array[:, :, 1] = np.clip(adjusted_image_array[:, :, 1] * g_adjust, 0, 255)  # Green
    adjusted_image_array[:, :, 2] = np.clip(adjusted_image_array[:, :, 2] * b_adjust, 0, 255)  # Blue

    # Convert back to an image
    adjusted_image = Image.fromarray(adjusted_image_array.astype('uint8'))

    # Save the adjusted image
    adjusted_image.save(output_path)