import os
import io
import glob
import json
import base64
import logging
#import functools
import asyncio
import aiofiles
from datetime import datetime, timedelta
from typing import Any, Dict, List, Optional, Tuple, Set
from pathlib import Path

import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.dates import AutoDateLocator, ConciseDateFormatter, DateFormatter
import mimetypes
from flask import (
    Flask, request, render_template, redirect, 
    url_for, jsonify, abort, Response
)
from flask_login import (
    LoginManager, UserMixin, 
    login_user, login_required, logout_user, current_user
)
from flask_limiter import Limiter
from flask_limiter.util import get_remote_address
from flask_talisman import Talisman
from werkzeug.middleware.proxy_fix import ProxyFix
try:
    from werkzeug.utils import escape
except:
    from werkzeug.security import escape

from config import ConfigApp, ConfigMain
from database import init_db, get_user_by_id, get_user_by_username, verify_user

try:
    import redis
    REDIS_AVAILABLE = True
except ImportError:
    REDIS_AVAILABLE = False

# Constants
ALLOWED_METHODS = ['GET', 'POST', 'HEAD', 'OPTIONS']
BLOCKED_PATHS = [
    '.env', '.git', 'config.json', 'secrets.yaml', 'secrets.yml',
    'php-cgi', 'wp-admin', 'adminer', 'backup', '.powenv', '.profile',
    'cgi-bin', 'bin/sh', 'eval-stdin.php', 'pearcmd', '.s3cfg',
    'vendor/phpunit', 'think/app', 'Util/PHP', '_info.php', '_phpinfo.php',
    'restore.php', 'backup.php', 'recovery.php', '_poopinfo.php',
    'wp-login.php', 'admin.php', 'config.php', '.phpinfo', '_profiler/info'
]
BLOCKED_KEYWORDS = [
    'allow_url_include', 'auto_prepend_file', 
    'call_user_func_array', 'invokefunction',
    'config-create', '/../../', '%2e%2e',
    'xdebug_session_start', 'phpstorm', 'debug'
]
BLOCKED_USER_AGENTS = [
    'nmap', 'sqlmap', 'nikto', 'metasploit', 'havij',
    'dirbuster', 'wpscan', 'acunetix', 'nessus',
    'firefox/14.0a1', 'firefox/44.0', 'chrome/3.0.195.17'
]
TRAVERSAL_PATTERNS = [
    '../', '..\\', '%2e%2e/', '%2e%2e%2f',
    '..;/', '..5c', '..%255c', '..%c0%af'
]
PHP_PATTERNS = [
    'php://', '<?php', '<?=', 'phar://', 'expect://',
    'zlib://', 'data://', 'glob://', 'ssh2://'
]
EXPLOIT_PATTERNS = [
    's=/index/\\think\\app/invokefunction',
    'function=call_user_func_array',
    'vars[0]=md5',
    'lang=../../../../../../../../'
]

# Logging configuration
logging.basicConfig(
    level=logging.INFO,
    handlers=[
        logging.FileHandler(
            ConfigMain.FLASK_LOGS_FILE_PATH, 
            encoding='utf-8',
            delay=True
        )
    ],
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)


class FileManager:
    """Manages file operations with thread safety and error handling"""
    
    @staticmethod
    def load_json(filepath: str) -> Dict[str, Any]:
        """Safely load JSON data from file"""
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                return json.load(f)
        except (FileNotFoundError, json.JSONDecodeError) as e:
            logger.error(f"JSON load error: {filepath} - {str(e)}")
            raise

    @staticmethod
    def save_json(filepath: str, data: Dict[str, Any]) -> None:
        """Atomically save JSON data to file"""
        try:
            temp_file = f"{filepath}.tmp"
            with open(temp_file, 'w', encoding='utf-8') as f:
                json.dump(data, f, ensure_ascii=False, indent=4)
            os.replace(temp_file, filepath)
        except OSError as e:
            logger.error(f"JSON save error: {filepath} - {str(e)}")
            raise

    @staticmethod
    def get_image_list(keep_count: Optional[int] = None) -> List[str]:
        """Get sorted list of JPG images with optional limit"""
        image_dir = ConfigMain.IMAGE_FOLDER
        try:
            images = sorted(
                glob.glob(os.path.join(image_dir, '*.jpg')),
                key=os.path.getmtime,
                reverse=False
            )
            if keep_count:
                images = images[-keep_count:]
            return [os.path.basename(img) for img in images]
        except OSError as e:
            logger.error(f"Image list error: {str(e)}")
            return []


class IPBlocker:
    """Менеджер блокировки IP-адресов"""
    def __init__(self):
        self._blocked_ips = set()
        self._last_modified = 0.0
        self._lock = asyncio.Lock()

    async def update_cache(self):
        """Обновление кэша заблокированных IP"""
        try:
            path = Path(ConfigMain.BLOCKED_IPS_FILE_PATH)
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
        if ip in ConfigApp.TRUSTED_IPS:
            return

        async with self._lock:
            try:
                async with aiofiles.open(ConfigMain.BLOCKED_IPS_FILE_PATH, 'a', encoding='utf-8') as f:
                    await f.write(f"{ip}\n")
                self._blocked_ips.add(ip)
                logging.warning(f"Заблокирован IP: {ip} - Причина: {reason}")
            except Exception as e:
                logging.error(f"Ошибка блокировки IP {ip}: {e}")       


class SecurityManager:
    """Handles security-related operations and threat detection"""
    
    def __init__(self):
        self.ip_blocker = IPBlocker()
        self.last_mtime: float = 0.0

    async def is_request_allowed(self, request) -> Tuple[bool, Optional[str]]:
        """Comprehensive security check for incoming requests"""
        client_ip = request.remote_addr
        
        # Check trusted IPs
        if client_ip in ConfigApp.TRUSTED_IPS:
            return True, None
            
        # Check blocked IPs cache
        if await self.ip_blocker.is_blocked(client_ip):
            logging.warning(self.ip_blocker._blocked_ips)
            return False, f"Blocked IP: {client_ip}"
 
        # Validate request components
        path = request.path.lower()
        query = request.query_string.decode().lower()
        
        # 1. Check blocked paths and keywords
        if (any(bp in path for bp in BLOCKED_PATHS) or
            any(bk in query for bk in BLOCKED_KEYWORDS)):
            return False, "Blocked path/query pattern"

        # 2. Check for traversal patterns
        if any(tp in path or tp in query for tp in TRAVERSAL_PATTERNS):
            return False, "Directory traversal attempt"

        # 3. Check for PHP injection patterns
        if any(php in query for php in PHP_PATTERNS):
            return False, "PHP injection attempt"

        # 4. Check for known exploits
        if any(ep in query for ep in EXPLOIT_PATTERNS):
            return False, "Known exploit pattern"

        # 5. Validate request data integrity
        if not self._validate_request_data(request):
            return False, "Invalid request data"

        # 6. Additional security checks
        if not self._validate_request_components(request):
            return False, "Invalid request components"

        return True, None

    def _validate_request_components(self, request) -> bool:
        """Validate various request components"""
        # HTTP method check
        if request.method not in ALLOWED_METHODS:
            return False
            
        # User-Agent validation
        ua = request.headers.get('User-Agent', '').lower()
        if any(bad_ua in ua for bad_ua in BLOCKED_USER_AGENTS):
            return False
            
        # URL length check
        if len(request.url) > 512:
            return False
            
        return True

    def _validate_request_data(self, request) -> bool:
        """Validate request data integrity"""
        try:
            # Check for malformed requests
            request.get_data()
            return True
        except Exception as e:
            logger.warning(f"Malformed request data from {request.remote_addr}: {str(e)}")
            return False


class PlotGenerator:
    """Handles generation of visualization plots with full legacy support"""
    
    THEME_CONFIG = {
        'dark': {
            'background': '#1a1a1a',
            'text': '#ffffff',
            'grid': '#404040',
            'spines': '#606060',
            'marker_edge': '#000000',
            'alarm_line_color': '#ff6b6b'
        },
        'light': {
            'background': '#ffffff',
            'text': '#000000',
            'grid': '#e0e0e0',
            'spines': '#cccccc',
            'marker_edge': '#ffffff',
            'alarm_line_color': '#e74c3c'
        }
    }
    
    @staticmethod
    def generate_plot(data: pd.DataFrame, 
                     timeframe: str,
                     chart_type: str,
                     theme: str = 'dark') -> Optional[str]:
        """Generate matplotlib plot image with theme support"""
        plt.ioff()
        theme_config = PlotGenerator.THEME_CONFIG.get(theme)
        
        with plt.style.context('dark_background' if theme == 'dark' else 'seaborn'):
            fig, ax = plt.subplots(figsize=(15, 5.5), facecolor=theme_config['background'])
            ax2 = None
            
            try:
                data['date'] = pd.to_datetime(data['date'], format='%d.%m.%Y %H:%M:%S')
                filtered_df, markersize = PlotGenerator._filter_data(data, timeframe)
                
                if filtered_df.empty:
                    return json.dumps({"error": "No data available"})
                
                # Агрегация данных для больших временных диапазонов
                if timeframe == 'last_year':
                    filtered_df = PlotGenerator._resample_data(filtered_df, 'D')
                
                chart_config = PlotGenerator._get_chart_config(chart_type, theme)
                if not chart_config:
                    return None
                
                ax2 = PlotGenerator._create_plot(ax, filtered_df, chart_config, markersize, theme_config)
                PlotGenerator._add_vertical_line(ax, filtered_df, data, theme_config)
                PlotGenerator._configure_axes(ax, ax2, filtered_df, chart_config, timeframe, theme_config)
                
                return PlotGenerator._save_plot_to_bytes(fig, theme_config)
            except Exception as e:
                logger.error(f"Plot generation error: {str(e)}")
                return None
            finally:
                plt.close(fig)
                plt.close('all')

    @staticmethod
    def _resample_data(df: pd.DataFrame, freq: str) -> pd.DataFrame:
        """Агрегация данных для больших временных диапазонов"""
        return df.resample(freq, on='date').mean().reset_index()

    @staticmethod
    def _filter_data(df: pd.DataFrame, timeframe: str) -> Tuple[pd.DataFrame, int]:
        """Фильтрация данных по временному диапазону"""
        now = datetime.now()
        markersize = 0
        
        timeframes = {
            'last_day': (timedelta(days=1), 4),
            'last_week': (timedelta(weeks=1), 0),
            'last_month': (timedelta(days=31), 0),
            'last_year': (timedelta(days=365), 0),
            'all': (None, 0)
        }
        
        if timeframe not in timeframes:
            raise ValueError("Invalid timeframe")
            
        delta, markersize = timeframes[timeframe]
        if delta:
            start_date = now - delta
            return df[df['date'] >= start_date], markersize
        return df, markersize

    @staticmethod
    def _get_chart_config(chart_type: str, theme: str) -> Dict[str, Any]:
        """Конфигурация с учетом темы"""
        color_palette = {
            'dark': {
                'temp_hue': {
                    'y1': ('Температура (°C)', '#ff6b6b'),  
                    'y2': ('Влажность (%)', '#4ecdc4')
                },
                'volt_sig': {
                    'y1': ('Вольтаж АКБ', '#ff9f43'), 
                    'y2': ('Сигнал GSM', '#2e86de')
                }
            },
            'light': {
                'temp_hue': {
                    'y1': ('Температура (°C)', '#e74c3c'),
                    'y2': ('Влажность (%)', '#3498db')
                },
                'volt_sig': {
                    'y1': ('Вольтаж АКБ', '#f39c12'), 
                    'y2': ('Сигнал GSM', '#27ae60')
                }
            }
        }
        
        configs = {
            'temp_hue': {
                'title': 'Температура и Влажность',
                'y1': ('temperature', *color_palette[theme]['temp_hue']['y1']),
                'y2': ('humidity', *color_palette[theme]['temp_hue']['y2'])
            },
            'volt_sig': {
                'title': 'Вольтаж АКБ и Сигнал GSM',
                'y1': ('voltage_akb', *color_palette[theme]['volt_sig']['y1']),
                'y2': ('signal', *color_palette[theme]['volt_sig']['y2'])
            }
        }
        return configs.get(chart_type)

    @staticmethod
    def _create_plot(ax, df: pd.DataFrame, config: Dict[str, Any], 
                    markersize: int, theme: Dict) -> plt.Axes:
        """Отрисовка с учетом темы"""
        # Оптимизация маркеров для больших данных
        use_markers = markersize > 0 and len(df) < 1000
        
        # Первая ось Y
        y1_col, y1_label, y1_color = config['y1']
        line1 = ax.plot(df['date'], df[y1_col],
                       marker='o' if use_markers else None,
                       markersize=markersize+2,
                       markeredgecolor=theme['marker_edge'],
                       linewidth=1.5,
                       alpha=0.9,
                       color=y1_color,
                       label=y1_label)[0]

        # Вторая ось Y
        ax2 = ax.twinx()
        y2_col, y2_label, y2_color = config['y2']
        line2 = ax2.plot(df['date'], df[y2_col],
                        marker='s' if use_markers else None,
                        markersize=markersize+1,
                        markeredgecolor=theme['marker_edge'],
                        linewidth=1.5,
                        alpha=0.9,
                        linestyle='--',
                        color=y2_color,
                        label=y2_label)[0]

        # Стилизация осей
        for axis in [ax, ax2]:
            axis.set_facecolor(theme['background'])
            axis.tick_params(axis='y', colors=theme['text'], labelsize=9)
            axis.yaxis.label.set_color(theme['text'])
        
        ax.set_title(config['title'], color=theme['text'], fontsize=14, pad=15)
        ax.grid(True, color=theme['grid'], linestyle=':', alpha=0.7)
        
        # Легенда
        ax.legend([line1, line2], [line1.get_label(), line2.get_label()],
                 loc='upper left', frameon=False,
                 fontsize=9, labelcolor=theme['text'])
        

        # Добавляем подписи осей
        ax.set_ylabel(config['y1'][1], color=config['y1'][2], fontsize=10, labelpad=5)
        ax2.set_ylabel(config['y2'][1], color=config['y2'][2], fontsize=10, labelpad=5)
        # ax.set_ylabel(config['y1'][1], color=theme['text'], fontsize=10, labelpad=5)
        # ax2.set_ylabel(config['y2'][1], color=theme['text'], fontsize=10, labelpad=5)
        
        return ax2
        
    @staticmethod
    def _add_vertical_line(ax, 
                          filtered_df: pd.DataFrame, 
                          full_df: pd.DataFrame,
                          theme: Dict) -> None:
        """Добавление вертикальной линии тревоги"""
        last_date = full_df['date'].max()
        current_time = datetime.now()
        time_diff = current_time - last_date
        
        if time_diff > timedelta(minutes=ConfigMain.CAMERA_UPDATE_MINUTES):
            # Параметры линии
            line_color = theme['alarm_line_color']
            line_style = '--'
            line_width = 2.5
            alpha = 0.8
            
            # Добавляем линию
            ax.axvline(x=current_time, 
                      color=line_color, 
                      linestyle=line_style,
                      linewidth=line_width,
                      alpha=alpha,
                      zorder=10)

    @staticmethod
    def _configure_axes(ax: plt.Axes, ax2: plt.Axes, 
                       df: pd.DataFrame, config: Dict[str, Any],
                       timeframe: str, theme: Dict) -> None:
        """Улучшенная настройка осей"""
        # Автоматическое форматирование дат
        locator = AutoDateLocator()
        formatter = ConciseDateFormatter(locator)
        ax.xaxis.set_major_locator(locator)
        ax.xaxis.set_major_formatter(formatter)
        
        # Настройка внешнего вида
        ax.tick_params(axis='x', colors=theme['text'], labelsize=9, rotation=30)
        ax.set_xlabel('')

        # Задаем цвет подписей осей Y
        ax.tick_params(axis='y', colors=config['y1'][2], labelsize=9)
        ax2.tick_params(axis='y', colors=config['y2'][2], labelsize=9)

        # Обновляем стиль подписей
        ax.xaxis.label.set_size(11)
        ax.yaxis.label.set_size(11)
        ax2.yaxis.label.set_size(11)
        
        # Добавляем отступы для подписей
        #ax.xaxis.set_label_coords(0.5, -0.15)
        ax.yaxis.set_label_coords(-0.05, 0.5)
        ax2.yaxis.set_label_coords(1.05, 0.5)

        # Динамические пределы Y
        for col, axis in zip([config['y1'][0], config['y2'][0]], [ax, ax2]):
            y_min = df[col].min()
            y_max = df[col].max()
            padding = (y_max - y_min) * 0.25 if y_max != y_min else abs(y_min)*0.25
            axis.set_ylim(y_min - padding, y_max + padding)

        # Стилизация рамок
        for spine in ax.spines.values():
            spine.set_color(theme['spines'])
            spine.set_linewidth(1)

        plt.tight_layout(pad=2)

    @staticmethod
    def _save_plot_to_bytes(fig, theme: Dict) -> bytes:
        """Сохранение с учетом темы"""
        img = io.BytesIO()
        fig.savefig(img, format='jpg', dpi=150, 
                  bbox_inches='tight', 
                  facecolor=theme['background'],
                  edgecolor=theme['background'])
        img.seek(0)
        return base64.b64encode(img.getvalue()).decode('utf-8')


# Flask application setup
app = Flask(__name__)
app.config.from_object(ConfigApp)

# Security middleware initialization
security = SecurityManager()
if ConfigApp.TALISMAN_ENABLED:
    Talisman(app, **ConfigApp.TALISMAN_CONFIG)

# Proxy configuration
if ConfigApp.USE_NGINX:
    app.wsgi_app = ProxyFix(
        app.wsgi_app, 
        x_for=1,
        x_proto=1,
        x_host=1,
        x_port=1
    )

# Rate limiter configuration
limiter = Limiter(
    app=app,
    key_func=get_remote_address,
    default_limits=["50 per day", "5 per hour"],
    storage_uri=ConfigApp.LIMITER_STORAGE_URI,
    storage_options=ConfigApp.LIMITER_STORAGE_OPTIONS,
    enabled=ConfigApp.LIMITER_ENABLED
)

def exempt_trusted_ips():
    return request.remote_addr in ConfigApp.TRUSTED_IPS


# Login manager setup
login_manager = LoginManager(app)
login_manager.login_view = 'login'
login_manager.session_protection = "strong"

class User(UserMixin):
    """Flask-Login user model"""
    def __init__(self, id: str, username: str):
        self.id = id
        self.username = username

@login_manager.user_loader
def load_user(user_id: str) -> Optional[User]:
    """User loader callback for Flask-Login"""
    user_data = get_user_by_id(user_id)
    return User(**user_data) if user_data else None


# Request hooks
@app.before_request
async def perform_security_checks():
    """Global security checks before request processing"""
    allowed, reason = await security.is_request_allowed(request)
    if not allowed:
        security.ip_blocker.block_ip(request.remote_addr, reason)
        logger.warning(f"Security violation: {reason}")
        abort(403)

@app.after_request
def apply_security_headers(response: Response) -> Response:
    """Apply security headers to all responses"""
    response.headers.extend({
        'Permissions-Policy': "geolocation=(), microphone=(), camera=(), payment=()",
        'Cross-Origin-Opener-Policy': 'same-origin',
        'Cache-Control': 'no-store, max-age=0'
    })
    return response


# Error handlers
@app.errorhandler(400)
def handle_bad_request(e):
    logger.warning(f"Bad request: {request.url}")
    return jsonify(error="Invalid request"), 400

@app.errorhandler(403)
def handle_forbidden(e):
    client_ip = request.remote_addr
    logger.warning(f"Forbidden access attempt from {client_ip}")
    return jsonify(error="Access denied"), 403


# Authentication routes
@app.route('/login', methods=['GET', 'POST'])
@limiter.limit("3 per minute")
def login():
    if current_user.is_authenticated:
        next_page = request.args.get('next')
        return redirect(next_page or url_for('settings'))

    if request.method == 'POST':
        username = escape(request.form.get('username', ''))
        password = request.form.get('password', '')

        if verify_user(username, password):
            user_data = get_user_by_username(username)
            login_user(User(user_data['id'],
                            user_data['username']), remember=True)
            logger.info(f"Successful login: {username}")
            
            next_page = request.args.get('next')
            return redirect(next_page or url_for('settings'))
            
        logger.warning(f"Failed login attempt: {username}")

    return render_template('login.html')

@app.route('/logout')
@login_required
def logout():
    logout_user()
    return redirect(url_for('login'))


# Добавляем правильные MIME-типы
mimetypes.add_type('application/javascript', '.js')
mimetypes.add_type('text/css', '.css')

# Кастомный обработчик статических файлов
@app.route('/static/<path:filename>')
def custom_static(filename):
    response = send_from_directory(app.static_folder, filename)
    
    if filename.endswith('.js'):
        response.headers.set('Content-Type', 'application/javascript')
    elif filename.endswith('.css'):
        response.headers.set('Content-Type', 'text/css')
        
    return response


# Application routes
@app.route('/')
@login_required
@limiter.limit("5 per minute", exempt_when=exempt_trusted_ips)
def index():
    try:
        images = FileManager.get_image_list(ConfigMain.DISPLAY_N_LAST_IMAGES)
        metadata = [parse_image_metadata(img) for img in images]
        return render_template('index.html', images=metadata)
    except Exception as e:
        logger.error(f"Index error: {str(e)}")
        abort(500)


@app.route('/settings')
@limiter.limit("5 per minute", exempt_when=exempt_trusted_ips)
@login_required
def settings():
    return render_template('settings.html')


@app.route('/api/settings', methods=['GET', 'POST'])
@limiter.limit("5 per minute", exempt_when=exempt_trusted_ips)
@login_required
def manage_settings():
    if request.method == 'GET':
        return jsonify(FileManager.load_json(ConfigMain.CAMERA_CONFIG_PATH))
        
    try:
        logger.info(f"Received settings update: {request.json}")
        current = FileManager.load_json(ConfigMain.CAMERA_CONFIG_PATH)
        update_settings(current, request.json)
        FileManager.save_json(ConfigMain.CAMERA_CONFIG_PATH, current)
        return jsonify(success=True)
    except Exception as e:
        logger.error(f"Settings error: {str(e)}")
        return jsonify(success=False, error=str(e)), 500


@app.route('/api/last_image')
@limiter.limit("5 per minute", exempt_when=exempt_trusted_ips)
def last_image() -> Dict[str, Any]:
    """Получение метаданных последнего изображения"""
    try:
        images = FileManager.get_image_list(ConfigMain.DISPLAY_N_LAST_IMAGES)
        if not images:
            return {'last_image': None}

        last_img = images[-1]
        metadata = parse_image_metadata(last_img)
        
        return {
            'images_count': len(images),
            'last_image': last_img,
            'voltage_sim': metadata['voltage_sim'],
            'voltage_akb': metadata['voltage_akb'],
            'signal_level': metadata['signal_level'],
            'reason': metadata['reason'],
            'date': metadata['date'],
            'temperature': metadata['temperature'],
            'humidity': metadata['humidity']
        }
    
    except IndexError as e:
        logger.error(f"Image metadata parsing error: {str(e)}")
        return {'last_image': None, 'error': 'Invalid image filename format'}
    except Exception as e:
        logger.error(f"Last image error: {str(e)}", exc_info=True)
        return {'last_image': None, 'error': 'Internal server error'}, 500



@app.route('/api/plot/<timeframe>/<chart_type>')
@limiter.limit("5 per minute", exempt_when=exempt_trusted_ips)
def generate_plot(timeframe, chart_type):
    """Генерация графиков с поддержкой старого формата"""
    try:
        df = load_sensors_data(ConfigMain.CSV_FILE_PATH)
        if df is None or df.empty:
            return jsonify(error="No data available"), 404

        result = PlotGenerator.generate_plot(df, timeframe, chart_type)
        
        if not result:
            return jsonify(error="Invalid chart type"), 400
            
        # Проверяем если результат содержит ошибку
        if isinstance(result, str) and "error" in result:
            return json.loads(result), 200

        return jsonify(image=f'data:image/jpg;base64,{result}')
    
    except Exception as e:
        logger.error(f"Plot generation failed: {str(e)}", exc_info=True)
        return jsonify(error="Internal server error"), 500


# Helper functions
def parse_image_metadata(filename: str) -> Dict[str, Any]:
    """Extract metadata from image filename"""
    parts = os.path.splitext(filename)[0].split('_')

    date_str = parts[0]
    time_str = parts[1]
    formatted_date = f"{date_str[6:8]}.{date_str[4:6]}.{date_str[0:4]}"
    formatted_time = f"{time_str[0:2]}:{time_str[2:4]}:{time_str[4:6]}"
    datetime_str = f"{formatted_date} {formatted_time}"

    try: user_id = str(parts[2])
    except: user_id = 'N/A'
    
    try: signal_level = int(int(parts[6].replace(',',''))/31*100)
    except: signal_level = 'N/A'
    
    try: reason = ConfigMain.WAKEUP_REASONS.get(parts[8], 'N/A')
    except: reason = 'N/A'
    
    return {
        'image': filename,
        'user_id': user_id,
        'date': datetime_str,
        'voltage_sim': safe_parse(parts, 4, float, 1000),
        'voltage_akb': safe_parse(parts, 14, float),
        'temperature': safe_parse(parts, 10, float),
        'humidity': safe_parse(parts, 12, float),
        'signal_level': signal_level,
        'reason': reason,
    }

def safe_parse(value: str, num: int, type_func, divisor: int = 1) -> Any:
    """Safely parse string values with error handling"""
    try:
        return type_func(value[num]) / divisor
    except (ValueError, IndexError, AttributeError):
        return 'N/A'

def load_sensors_data(csv_file_path):
    # Load the CSV file into a DataFrame
    if os.path.isfile(csv_file_path):
        df = pd.read_csv(csv_file_path)
        return df
    else:
        logger.error(f"CSV file does not exist. File:{csv_file_path}")
        return None     
                
def update_settings(current: Dict, updates: Dict) -> None:
    """Рекурсивно обновляет настройки, сохраняя типы."""
    for key, value in updates.items():
        if isinstance(value, dict) and key in current:
            # Рекурсивный вызов для вложенных словарей
            update_settings(current[key], value)
        else:
            # Обновляем значение с проверкой типа
            if key in current:
                if isinstance(current[key], dict) and 'current' in current[key]:
                    # Для настроек с полем 'current'
                    target_type = type(current[key]['current'])
                    try:
                        current[key]['current'] = target_type(value)
                    except (TypeError, ValueError):
                        logger.error(f"Type error in {key}: {value}")
                else:
                    # Прямое обновление
                    target_type = type(current[key])
                    try:
                        current[key] = target_type(value)
                    except (TypeError, ValueError):
                        logger.error(f"Type error in {key}: {value}")
            else:
                # Новая настройка (не рекомендуется)
                current[key] = value

def test_redis_connection():
    try:
        redis_url = os.getenv('REDIS_URL')
        if redis_url is None:
            print(f"Redis url is not defined!")
            return False
        redis_client = redis.from_url(redis_url, socket_connect_timeout=5)
        redis_client.ping()
        print(f"Redis connection ok")
        return True
    except (redis.ConnectionError, ValueError) as e:
        print(f"Redis connection error: {e}")
        return False
        

# Initialization
def initialize_app():
    """Application initialization routine"""
    os.makedirs(ConfigMain.IMAGE_FOLDER, exist_ok=True)
    os.makedirs(os.path.dirname(ConfigMain.CSV_FILE_PATH), exist_ok=True)
    init_db()
    
    if REDIS_AVAILABLE and ConfigApp.USE_REDIS:
        test_redis_connection()


if __name__ == '__main__':
    initialize_app()
    ssl_context = (
        ConfigApp.CERT_FILE, 
        ConfigApp.KEY_FILE
    ) if ConfigApp.RUN_WITH_SSL else None
    
    app.run(
        host='0.0.0.0',
        port=ConfigApp.APP_PORT,
        ssl_context=ssl_context,
        debug=ConfigApp.DEBUG
    )