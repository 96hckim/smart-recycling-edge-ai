"""API 계층 간 데이터 교환 및 유효성 검증을 위한 Pydantic DTO 정의 모듈."""

from pydantic import BaseModel, Field


# ----------------------------------------------------------------------------
# 1. 회원 인증 및 사용자 정보
# ----------------------------------------------------------------------------
class LoginRequest(BaseModel):
    """모바일 간편 로그인 요청 DTO."""

    phone: str = Field(..., description="휴대폰 번호 (예: '01012345678')", min_length=1)
    name: str | None = Field(default="회원", examples=["홍길동"])


class UserResponse(BaseModel):
    """사용자 프로필 및 잔여 포인트 응답 DTO."""

    id: int
    phone: str
    name: str | None = "회원"
    points: int
    created_at: str


# ----------------------------------------------------------------------------
# 2. 키오스크 바인딩 (QR 세션)
# ----------------------------------------------------------------------------
class KioskBindRequest(BaseModel):
    """모바일 앱의 키오스크 QR 스캔 바인딩 요청 DTO."""

    bin_id: int = Field(..., description="연동할 키오스크 ID")
    user_id: int = Field(..., description="QR을 스캔한 유저 ID")


class KioskBindResponse(BaseModel):
    """키오스크 바인딩 결과 응답 DTO."""

    status: str = "SUCCESS"
    message: str
    bin_id: int
    user_id: int


# ----------------------------------------------------------------------------
# 3. 분리배출 투입 정산
# ----------------------------------------------------------------------------
class RecycleSubmitRequest(BaseModel):
    """키오스크 배출 완료 데이터 제출 요청 DTO."""

    bin_id: int
    user_id: int | None = Field(None, description="비회원일 경우 null")
    can_count: int = Field(0, ge=0)
    pet_count: int = Field(0, ge=0)
    paper_count: int = Field(0, ge=0)
    vinyl_count: int = Field(0, ge=0)
    carbon_saved_g: float = Field(0.0, ge=0.0)
    earned_points: int = Field(0, ge=0)


class RecycleSubmitResponse(BaseModel):
    """정산 완료 및 포인트 적립 결과 응답 DTO."""

    status: str = "SUCCESS"
    log_id: int
    earned_points: int
    total_points: int | None = None


# ----------------------------------------------------------------------------
# 4. 배출 이력 조회
# ----------------------------------------------------------------------------
class RecycleLogItem(BaseModel):
    """단일 배출 상세 로그 모델."""

    id: int
    bin_id: int
    can_count: int
    pet_count: int
    paper_count: int
    vinyl_count: int
    carbon_saved_g: float
    earned_points: int
    created_at: str


class RecycleLogListResponse(BaseModel):
    """사용자별 배출 이력 목록 응답 DTO."""

    user_id: int
    total_count: int
    logs: list[RecycleLogItem]


# ----------------------------------------------------------------------------
# 5. 포인트 차감 (상품 교환)
# ----------------------------------------------------------------------------
class PointDeductRequest(BaseModel):
    """포인트 차감/사용 요청 DTO."""

    user_id: int = Field(..., description="포인트를 차감할 유저 ID")
    amount: int = Field(..., gt=0, description="차감할 포인트 (0보다 큰 정수)")
    description: str | None = Field(default="상품 교환", description="차감 사유/상품명")


class PointDeductResponse(BaseModel):
    """포인트 차감 처리 결과 응답 DTO."""

    status: str = "SUCCESS"
    user_id: int
    deducted_amount: int
    remaining_points: int
    description: str
