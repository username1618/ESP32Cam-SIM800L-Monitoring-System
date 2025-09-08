#!/bin/bash

# Переходим в корень проекта
cd "$(dirname "$0")"

# Собираем образ из директории sim800, используя корень проекта как контекст
docker build -t service/sim800 \
  -f app_sim800/Dockerfile_sim800 \
  .