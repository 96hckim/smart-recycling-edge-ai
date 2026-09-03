import uvicorn
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.config import settings
from app.routers import auth, kiosk, users, websocket

# FastAPI 인스턴스 초기화
app = FastAPI(
    title="Smart Recycling Central Server",
    description="스마트 분리배출 키오스크(Qt) & 모바일 앱(Android) 실시간 WebSocket 연동 백엔드",
    version="1.0.0",
)

# CORS 미들웨어 설정 (모바일 앱 및 외부 웹 대시보드 통신 허용)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # 로컬 개발 및 테스트 환경용 전체 개방
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# 라우터 등록
app.include_router(websocket.router)
app.include_router(auth.router)
app.include_router(kiosk.router)
app.include_router(users.router)


@app.get("/health", tags=["System"])
def health_check() -> dict[str, str]:
    """서버 헬스체크용 엔드포인트."""
    return {"status": "OK", "service": "Smart Recycling Backend"}


if __name__ == "__main__":
    # main.py 직접 실행 지원 (개발 환경 편의)
    uvicorn.run(
        "main:app",
        host=settings.HOST,
        port=settings.PORT,
        reload=True,
    )
