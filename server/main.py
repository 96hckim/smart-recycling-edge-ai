"""FastAPI 애플리케이션 진입점, Lifespan 라이프사이클 및 라우터 마운트 모듈."""

from collections.abc import AsyncGenerator
from contextlib import asynccontextmanager

import uvicorn
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.config import settings
from app.database import init_db
from app.exceptions import register_exception_handlers
from app.routers import auth, kiosk, users, websocket


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncGenerator[None, None]:
    """애플리케이션 수명 주기 관리 (기동 시 DB 테이블 및 WAL 모드 인덱스 자동 생성)."""
    await init_db()
    yield


# FastAPI 애플리케이션 초기화
app = FastAPI(
    title="Smart Recycling Central Server",
    description="스마트 분리배출 키오스크(Qt) & 모바일 앱(Android) 실시간 WebSocket 연동 백엔드",
    version="2.0.0",
    lifespan=lifespan,
)

# 외부 클라이언트(Qt/Android) 호환을 위한 전역 표준 JSON 예외 처리기 등록
register_exception_handlers(app)

# 로컬 개발 및 교차 출처 클라이언트 통신을 위한 CORS 미들웨어 구성
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# 서브 도메인별 라우터 계층 등록
app.include_router(websocket.router)
app.include_router(auth.router)
app.include_router(kiosk.router)
app.include_router(users.router)


@app.get("/health", tags=["System"])
def health_check() -> dict[str, str]:
    """로드밸런서 및 외부 모니터링 연동용 서버 상태 진단 엔드포인트."""
    return {"status": "OK", "service": "Smart Recycling Backend"}


if __name__ == "__main__":
    # 개발 편의를 위한 Uvicorn ASGI 비동기 서버 직접 구동 엔트리포인트
    uvicorn.run(
        "main:app",
        host=settings.HOST,
        port=settings.PORT,
        reload=True,
    )
