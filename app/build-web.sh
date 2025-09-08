#!/bin/bash

cd "$(dirname "$0")"

docker build -t service/web \
  -f app_web/Dockerfile_web \
  .