"""회원 가입 및 간편 로그인 비즈니스 로직 계층 모듈."""

from app.exceptions import AppException
from app.repositories.user_repository import UserRepository
from app.schemas import UserResponse


class AuthService:
    """전화번호 기반 계정 생성 및 조회 워크플로우 처리 서비스."""

    def __init__(self, user_repo: UserRepository) -> None:
        self.user_repo = user_repo

    async def login_or_register(
        self, phone: str, name: str | None = None
    ) -> UserResponse:
        """입력값 정제 후 DB UPSERT를 통해 회원 계정을 생성하거나 기존 정보를 반환."""
        clean_phone = phone.strip()
        clean_name = name.strip() if name and name.strip() else "회원"

        if not clean_phone:
            raise AppException(
                status_code=400,
                detail="휴대폰 번호는 필수 입력 항목입니다.",
                error_code="INVALID_PHONE",
            )

        row = await self.user_repo.upsert_user(clean_phone, clean_name)
        if not row:
            raise AppException(
                status_code=500,
                detail="유저 데이터 생성 또는 조회에 실패했습니다.",
                error_code="USER_CREATION_FAILED",
            )

        return UserResponse(
            id=row["id"],
            phone=row["phone"],
            name=row["name"],
            points=row["points"],
            created_at=str(row["created_at"]),
        )
