from pydantic import BaseModel, Field


# ============================================================================
# 1. 회원 인증 관련 (Auth & User)
# ============================================================================
class LoginRequest(BaseModel):
    phone: str = Field(..., description="휴대폰 번호 (예: '01012345678')")
    name: str | None = Field(default="회원", example="홍길동")


class UserResponse(BaseModel):
    id: int
    phone: str
    name: str | None = "회원"
    points: int
    created_at: str


# ============================================================================
# 2. 키오스크 바인딩 (QR 세션)
# ============================================================================
class KioskBindRequest(BaseModel):
    bin_id: int = Field(..., description="연동할 키오스크 ID")
    user_id: int = Field(..., description="QR을 스캔한 유저 ID")


class KioskBindResponse(BaseModel):
    status: str = "SUCCESS"
    message: str
    bin_id: int
    user_id: int


# ============================================================================
# 3. 분리배출 투입 정산 (Recycle Processing)
# ============================================================================
class RecycleSubmitRequest(BaseModel):
    bin_id: int
    user_id: int | None = Field(None, description="비회원일 경우 null")
    can_count: int = Field(0, ge=0)
    pet_count: int = Field(0, ge=0)
    paper_count: int = Field(0, ge=0)
    vinyl_count: int = Field(0, ge=0)
    carbon_saved_g: float = Field(0.0, ge=0.0)
    earned_points: int = Field(0, ge=0)


class RecycleSubmitResponse(BaseModel):
    status: str = "SUCCESS"
    log_id: int
    earned_points: int
    total_points: int | None = None


# ============================================================================
# 4. 배출 이력 조회 (History Logs)
# ============================================================================
class RecycleLogItem(BaseModel):
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
    user_id: int
    total_count: int
    logs: list[RecycleLogItem]
