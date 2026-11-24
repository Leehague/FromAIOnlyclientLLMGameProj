# 잿빛 왕관의 연대기 (Chronicle of the Ashen Crown)

C++17과 `raylib`(+ `nlohmann/json`)으로 제작된 **클라이언트 온리 2D 탑뷰 타일맵 솔로 RPG**입니다.

## 주요 특징
- **싱글톤 통합 Manager 아키텍처**: 단일 싱글톤 `GameManager`가 공통 추상 인터페이스 `IManager`를 구현한 8개 서브 매니저(`Data`, `Resource`, `Map`, `Economy`, `Quest`, `Entity`, `Combat`, `UI`)의 생명주기와 호출을 통합 관리합니다.
- **100% 데이터 드리븐 로딩**:
  - 기획 문서([docs/game_design.md](docs/game_design.md), [docs/tile_design.md](docs/tile_design.md), [docs/art_resources.md](docs/art_resources.md))
  - JSON 스펙 데이터(`data/*.json`) — 플레이어, 아이템, 몬스터, 보스, NPC, 퀘스트, 맵, 타일
  - 타일 번호 배열 TXT 맵(`data/maps/town.txt`, `field.txt`, `boss_room.txt`)
  - 아트 리소스 번호 ↔ 경로 매핑 파일(`data/art_mapping.txt`)
- **8방향 이동 및 방향별 애니메이션**: 상/하/좌/우 + 4대각선 이동 및 8방향 × 4프레임(`Idle`, `Walk`, `Attack`) 스프라이트 시트 애니메이션.
- **3종 맵 + RPG 핵심 시스템**: 마을(안전지대/NPC 3종/상점/회복) → 필드(슬라임·고블린·해골 전투/리스폰) → 보스 룸(잿빛 군주 말라코르 2페이즈 패턴 전투), 퀘스트 체인, 골드/상점/장비 시스템.

## 빌드 및 실행 방법 (Windows + Visual Studio 2022)
```powershell
# 빌드 및 자동 검증(스모크 테스트)
powershell -ExecutionPolicy Bypass -File build.ps1 -SmokeTest

# 빌드 및 게임 실행
powershell -ExecutionPolicy Bypass -File build.ps1 -Run
```
빌드 완료 후 `build\Release\AshenCrownRPG.exe`를 직접 실행할 수도 있습니다.

## 조작키
- **이동**: `WASD` 또는 방향키 (8방향)
- **기본 검격**: `J` / `Space` / 마우스 좌클릭
- **회오리 베기 스킬**: `K` / 마우스 우클릭
- **회피 대시**: `Left Shift`
- **NPC 상호작용 (퀘스트/상점/회복)**: `E`
- **포션 퀵슬롯**: `1` (체력 포션), `2` (상급 체력 포션)
- **인벤토리/장비창**: `I` | **퀘스트 일지**: `Q` | **창 닫기**: `ESC`
