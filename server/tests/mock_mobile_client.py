import asyncio
import json

import httpx
import websockets

# 테스트 대상 서버 및 키오스크 ID 설정
SERVER_HOST = "127.0.0.1"
SERVER_PORT = 8000
BIN_ID = 1
MOCK_PHONE = "010-1234-5678"

HTTP_BASE_URL = f"http://{SERVER_HOST}:{SERVER_PORT}"
WS_URL = f"ws://{SERVER_HOST}:{SERVER_PORT}/ws/kiosk/{BIN_ID}/mobile"


async def main() -> None:
    print("\n=======================================================")
    print(" 📱 모바일 앱 가상 시뮬레이터 (Mock Mobile Client)")
    print("=======================================================\n")

    async with httpx.AsyncClient(base_url=HTTP_BASE_URL) as client:
        # [Step 1] 모바일 앱 간이 로그인 (DB에 번호가 없으면 자동 가입)
        print(f"[1단계] 회원 간이 로그인 시도 (Phone: {MOCK_PHONE})...")
        login_resp = await client.post("/api/auth/login", json={"phone": MOCK_PHONE})

        if login_resp.status_code != 200:
            print(f"❌ 로그인 실패: {login_resp.status_code} {login_resp.text}")
            return

        user_data = login_resp.json()
        user_id = user_data["id"]
        points = user_data["points"]
        print(f"✅ 로그인 성공! (User ID: {user_id}, 현재 포인트: {points}P)\n")

        # [Step 2] 모바일 전용 WebSocket 방 입장 (키오스크 배출 완료 실시간 수신용)
        print(f"[2단계] 모바일 WebSocket 방 접속 중 -> {WS_URL}")
        async with websockets.connect(WS_URL) as ws:
            print(f"✅ WebSocket 방 접속 완료! (Bin ID: {BIN_ID} 대기실)\n")

            # [Step 3] QR 코드 스캔 트리거
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
                "✅ 바인딩 성공! Qt 키오스크 화면이 '투입 화면'으로 전환되었는지 확인하세요."
            )
            print(
                "⏳ 키오스크에서 품목 투입 후 [투입 완료] 버튼을 누를 때까지 대기합니다...\n"
            )

            # [Step 4] 키오스크가 정산을 끝냈을 때 날아오는 RECYCLE_COMPLETE 대기
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

        # [Step 5] DB 저장 데이터 최종 확인
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
