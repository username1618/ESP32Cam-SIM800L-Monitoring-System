import sqlite3
import bcrypt
from contextlib import contextmanager

DATABASE_NAME = 'data/users.db'

def init_db():
    with db_connection() as conn:
        conn.execute('''
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL
        )
        ''')
        conn.commit()

@contextmanager
def db_connection():
    conn = sqlite3.connect(DATABASE_NAME)
    conn.row_factory = sqlite3.Row  # Для доступа к полям по имени
    try:
        yield conn
    finally:
        conn.close()

def add_user(username, password):
    password_hash = bcrypt.hashpw(password.encode('utf-8'), bcrypt.gensalt())
    with db_connection() as conn:
        try:
            conn.execute(
                'INSERT INTO users (username, password_hash) VALUES (?, ?)',
                (username, password_hash)
            )
            conn.commit()
            return True
        except sqlite3.IntegrityError:
            return False

def get_user_by_id(user_id):
    with db_connection() as conn:
        cursor = conn.execute(
            'SELECT id, username FROM users WHERE id = ?',
            (user_id,))
        return cursor.fetchone()

def get_user_by_username(username):
    with db_connection() as conn:
        cursor = conn.execute(
            'SELECT id, username, password_hash FROM users WHERE username = ?',
            (username,))
        return cursor.fetchone()

def verify_user(username, password):
    user = get_user_by_username(username)
    if user:
        # Правильное обращение к полям Row-объекта
        return bcrypt.checkpw(password.encode('utf-8'), user['password_hash'])
    return False