"""모바일 앱 인증, QR 스캔 세션 바인딩 및 실시간 정산 수신 E2E 통합 테스트 시뮬레이터."""

import asyncio
import json

import httpx
import websockets

# 테스트 환경 접속 파라미터
SERVER_HOST = "127.0.0.1"
SERVER_PORT = 8000
BIN_ID = 1
MOCK_PHONE = "010-1234-5678"
MOCK_NAME = "Admin"

HTTP_BASE_URL = f"http://{SERVER_HOST}:{SERVER_PORT}"
WS_URL = f"ws://{SERVER_HOST}:{SERVER_PORT}/ws/kiosk/{BIN_ID}/mobile"


async def main() -> None:
    """HTTP 및 WebSocket 채널을 결합한 모바일-키오스크 연동 시나리오 전체 검증."""
    print("\n=======================================================")
    print(" 📱 모바일 앱 가상 시뮬레이터 (Mock Mobile Client)")
    print("=======================================================\n")

    async with httpx.AsyncClient(base_url=HTTP_BASE_URL) as client:
        # 1. 회원 간이 로그인 및 프로필 획득
        print(
            f"[1단계] 회원 간이 로그인 시도 (Phone: {MOCK_PHONE}, Name: {MOCK_NAME})..."
        )
        login_resp = await client.post(
            "/api/auth/login",
            json={"phone": MOCK_PHONE, "name": MOCK_NAME},
        )

        if login_resp.status_code != 200:
            print(f"❌ 로그인 실패: {login_resp.status_code} {login_resp.text}")
            return

        user_data = login_resp.json()
        user_id = user_data["id"]
        user_name = user_data.get("name", "회원")
        points = user_data["points"]
        print(
            f"✅ 로그인 성공! (User ID: {user_id}, 이름: {user_name}, 현재 포인트: {points}P)\n"
        )

        # 2. 키오스크 1:1 대응 모바일 전용 WebSocket 채널 구독
        print(f"[2단계] 모바일 WebSocket 방 접속 중 -> {WS_URL}")
        async with websockets.connect(WS_URL) as ws:
            print(f"✅ WebSocket 방 접속 완료! (Bin ID: {BIN_ID} 대기실)\n")

            # 3. 키오스크 화면의 QR 코드 스캔 트리거 (바인딩 API 호출)
            input("👉 [Enter]를 누르면 키오스크 화면의 QR 코드를 스캔합니다...")
            print("\n[3단계] QR 바인딩 요청 전송 (POST /api/kiosk/bind)...")

            bind_resp = await client.post(
                "/api/kiosk/bind",
                json={"bin_id": BIN_ID, "user_id": user_id},
            )

            if bind_resp.status_code != 200:
                print(f"❌ 바인딩 실패: {bind_resp.status_code} {bind_resp.text}")
                return

            print(
                f"✅ 바인딩 성공! Qt 키오스크 화면에 '{user_name}' 님 환영 문구와 함께 '투입 화면'으로 전환되었는지 확인하세요."
            )
            print(
                "⏳ 키오스크에서 품목 투입 후 [투입 완료] 버튼을 누를 때까지 대기합니다...\n"
            )

            # 4. 키오스크 투입 완료 시 서버가 브로드캐스트하는 RECYCLE_COMPLETE 이벤트 폴링
            while True:
                msg = await ws.recv()
                event_data = json.loads(msg)

                if event_data.get("event") == "RECYCLE_COMPLETE":
                    print("🎉 [서버로부터 실시간 배출 정산 이벤트 수신!]")
                    print(f" - 종이 수량 : {event_data.get('paper_count')}개")
                    print(f" - 캔 수량   : {event_data.get('can_count')}개")
                    print(f" - 페트 수량 : {event_data.get('pet_count')}개")
                    print(f" - 비닐 수량 : {event_data.get('vinyl_count')}개")
                    print(f" - 절감 탄소 : {event_data.get('carbon_saved_g')}g CO₂")
                    print(f" - 획득 포인트: +{event_data.get('earned_points')}P")
                    print(f" - 누적 포인트: {event_data.get('total_points')}P")
                    break

        # 5. DB 영속성 검증 (저장된 정산 이력 REST API 조회)
        print("\n[5단계] 백엔드 DB 이력(GET /api/users/{id}/logs) 최종 확인 중...")
        logs_resp = await client.get(f"/api/users/{user_id}/logs")
        if logs_resp.status_code == 200:
            logs = logs_resp.json()
            print(
                f"✅ DB 기록 검증 완료 (해당 계정에 총 {logs['total_count']}건의 로그 존재)"
            )

        print("\n✨ 전체 통신 시나리오가 정상 동작함을 확인했습니다!")


if __name__ == "__main__":
    asyncio.run(main())
