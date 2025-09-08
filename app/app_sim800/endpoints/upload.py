import logging
from fastapi import APIRouter, UploadFile, File, HTTPException, Request
from fastapi.responses import Response

from config import Settings
from utilits import process_upload, validate_filename
from limiter import limiter


config = Settings()
upload_router = APIRouter()

@upload_router.post("/upload")
@limiter.limit("50/day")
@limiter.limit("5/hour")
async def upload_image(
    request: Request,
    file: UploadFile = File(...)
):
    """Эндпоинт для загрузки изображений"""
    print("Запрос принят! upload_image", request.headers.get("X-Forwarded-For"))
    try:
        if not validate_filename(file.filename, config):
            raise HTTPException(
                status_code=400,
                detail="Invalid file"
            )
        await process_upload(file, config)
        
        return Response(status_code=200)
        #return {"status": "success", "filename": file.filename}

    except Exception as e:
        logging.error(f"Upload failed: {str(e)}", exc_info=True)
        raise HTTPException(
            status_code=500,
            detail="File processing error"
        )        