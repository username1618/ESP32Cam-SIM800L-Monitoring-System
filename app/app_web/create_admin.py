from database import init_db, add_user

if __name__ == '__main__':
    init_db()
    if add_user('admin', 'admin_password'):
        print("Admin user created successfully")
    else:
        print("Admin user already exists or error occurred")