# 기획자용 통합 툴 기능 목록 및 CLI 레퍼런스 (`tools/designer_tool.py`)

이 문서는 《잿빛 왕관의 연대기》 기획자용 툴에 구현된 **모든 기능(UI + CLI 공통)**의 전체 목록과 CLI 실행 규격을 정의합니다.  
UI(`tools/designer_ui.py`)와 CLI(`tools/designer_tool.py`)는 동일한 코어 모듈(`tools/designer_core.py`)을 호출하므로 동작과 데이터 포맷이 100% 일치합니다.

---

## 0. 실행 진입점 및 공통 옵션
* **데스크톱 UI 실행**:
  ```powershell
  python tools/designer_tool.py ui
  # 또는 원클릭 실행 스크립트
  powershell -ExecutionPolicy Bypass -File launch_designer_tool.ps1
  ```
* **CLI 실행 기본 형식**:
  ```powershell
  python tools/designer_tool.py [--root <경로>] <카테고리> <서브커맨드> [옵션] [--json]
  ```
  * 모든 CLI 명령은 `--json` 플래그를 지원합니다. (LLM 자동화 및 스크립트 파싱 시 `--json` 권장)
  * `--root <경로>`: 테스트나 별도 워크스페이스에서 구동할 때 프로젝트 루트를 지정합니다.
* **UI 셀프 테스트 (헤드리스 점검)**:
  ```powershell
  python tools/designer_tool.py ui --self-test
  ```
* **전체 프로젝트 데이터 교차 무결성 검증**:
  ```powershell
  python tools/designer_tool.py validate [--json]
  ```
  * 모든 `data/*.json`, `data/maps/*.txt`, `data/art_mapping.txt`의 ID 참조·파일 존재·스폰/워프 좌표 통행 가능 여부를 일괄 검증합니다.

---

## 1. 맵 제작 및 수정 (`map`)

### 구현된 기능 목록 (UI 탭 1 + CLI)
| 기능 | UI 동작 | CLI 명령어 |
| :--- | :--- | :--- |
| 맵 목록 조회 | 상단 `맵 선택` 드롭다운 | `map list [--json]` |
| 맵 타일 배열 조회 | 중앙 PNG 타일 캔버스 렌더링 | `map show <map_id> [--json]` |
| 새 맵 생성 | `새 맵 생성` 다이얼로그 (`data/maps/<id>.txt` + `maps.json` 자동 등록) | `map create --id <id> --name <이름> [--type town\|field\|boss] [--width 24] [--height 18] [--fill-tile 0] [--border-tile 6]` |
| 맵 삭제 | — | `map delete --id <id> [--keep-txt]` |
| 단일 타일 수정 | 타일 팔레트 선택 후 좌클릭/드래그 (우클릭: 스포이드, Undo 지원) | `map set-tile --id <id> --x <x> --y <y> --tile <tile_id>` |
| 사각형 영역 채우기 | — | `map fill --id <id> --x0 <x0> --y0 <y0> --x1 <x1> --y1 <y1> --tile <tile_id>` |
| 연속 영역 채우기 (Flood Fill) | `채우기(Flood Fill)` 모드 클릭 | `map flood-fill --id <id> --x <x> --y <y> --tile <tile_id>` |
| 맵 크기 조절 | `크기 조절` 버튼 | `map resize --id <id> --width <w> --height <h> [--fill-tile 0]` |
| 맵/워프/스폰 무결성 검증 | `검증` 버튼 | `map validate [--json]` |

### CLI 예시
```powershell
python tools/designer_tool.py map list --json
python tools/designer_tool.py map create --id dark_cave --name "어둠의 동굴" --type field --width 20 --height 16 --fill-tile 3 --border-tile 9 --json
python tools/designer_tool.py map fill --id dark_cave --x0 2 --y0 2 --x1 5 --y1 5 --tile 11 --json
python tools/designer_tool.py map set-tile --id dark_cave --x 10 --y 8 --tile 14 --json
python tools/designer_tool.py map validate --json
```

---

## 2. 아트 리소스 업로드 및 매핑 관리 (`art`)

### 카테고리 및 자동 Art ID 대역
`tiles`(1~99), `characters`(101~199), `monsters`(201~299), `boss`(301~399), `npcs`(401~499), `items`(501~599), `effects`(601~699)

### 구현된 기능 목록 (UI 탭 2 + CLI)
| 기능 | UI 동작 | CLI 명령어 |
| :--- | :--- | :--- |
| 매핑된 아트 목록 & 규격 조회 | 좌측 갤러리 목록 + PNG 미리보기 + 카테고리 필터 | `art list [--category all\|tiles\|characters\|monsters\|boss\|npcs\|items\|effects] [--json]` |
| 이미지 업로드 & 자동 등록 | 파일 선택 → PNG 변환/리사이즈 → `assets/<카테고리>/` 저장 → `data/art_mapping.txt` & `docs/art_resources.md` 자동 갱신 (+ 대기중 요구사항 완료 연결 가능) | `art upload --file <경로> --category <카테고리> [--id <art_id>] [--name <파일명>] [--resize WxH] [--concept <설명>] [--fulfill-request REQ-001] [--overwrite] [--json]` |
| 아트 매핑 제거 | — | `art remove --id <art_id> [--delete-file] [--json]` |
| 아트 파일 누락 검증 | `무결성 검증` 버튼 | `art verify [--json]` |

### CLI 예시
```powershell
python tools/designer_tool.py art list --category items --json
python tools/designer_tool.py art upload --file C:\tmp\ice_sword.png --category items --name item_sword_ice --resize 32x32 --concept "얼음 결정이 박힌 장검 아이콘" --fulfill-request REQ-001 --json
python tools/designer_tool.py art verify --json
```

---

## 3. 아트 리소스 요구사항 기록 — LLM 이미지 생성 준비 큐 (`art-req`)

저장 파일: `data/art_requests.json`  
상태 값: `pending`(**아직 없지만 요구된 상태**), `fulfilled`(생성/업로드 완료), `cancelled`(취소)

### 구현된 기능 목록 (UI 탭 3 + CLI)
| 기능 | UI 동작 | CLI 명령어 |
| :--- | :--- | :--- |
| 요구사항 목록 조회 (기본: `pending`) | 상태 라디오 필터 + 검색창 + `LLM용 Pending JSON 미리보기` | `art-req list [--status pending\|fulfilled\|cancelled\|all] [--json]` |
| 신규 요구사항 등록 (`pending`) | 제목·카테고리·규격·권장 Art ID·짧은 설명 입력 후 `신규 등록` | `art-req add --title <제목> --prompt <짧은 설명> [--category items] [--size 32x32] [--art-id <id>] [--target-path <경로>] [--notes <메모>] [--json]` |
| 요구사항 수정 / 상태 변경 | 선택 항목 편집 후 `선택 항목 수정 저장` | `art-req update --id REQ-001 [--title ...] [--prompt ...] [--category ...] [--size ...] [--status pending\|fulfilled\|cancelled] [--notes ...] [--json]` |
| 완료 처리 (Art ID 연결) | 업로드 탭의 `요구사항 완료 연결` 또는 상태 변경 | `art-req fulfill --id REQ-001 --art-id <art_id> [--path <최종경로>] [--json]` |
| 요구사항 삭제 | `삭제` 버튼 | `art-req delete --id REQ-001 [--json]` |

### LLM 자동화 워크플로우 (대기중 아트 일괄 생성 시)
1. `python tools/designer_tool.py art-req list --status pending --json`으로 아직 생성되지 않은(`pending`) 아트 목록과 `prompt`, `category`, `size`, `suggested_art_id`, `target_path` 확인.
2. 각 항목의 `prompt`와 `size`를 바탕으로 이미지 생성 모델 호출하여 이미지 생성.
3. `python tools/designer_tool.py art upload --file <생성된이미지> --category <category> --id <suggested_art_id> --resize <size> --concept "<prompt>" --fulfill-request <request_id> --json` 실행 → `assets/` 저장, `data/art_mapping.txt` 등록, `docs/art_resources.md` 기록, `REQ-XXX`의 `fulfilled` 전환까지 한 번에 완료.

---

## 4. 기획 문서 편집기 — JSON 파싱 · 검색 · 수정 (`doc`)

지원 문서 키(`--doc`): `items`, `monsters`, `bosses`, `npcs`, `quests`, `maps`, `tiles`, `player`, `strings`

### 구현된 기능 목록 (UI 탭 4 + CLI)
| 기능 | UI 동작 | CLI 명령어 |
| :--- | :--- | :--- |
| 기획 JSON 목록 & 항목 수 조회 | 상단 `기획 문서(JSON)` 콤보박스 | `doc list [--json]` |
| 키워드 검색 (단일 / 전체 문서 통합) | 상단 검색창 + `전체 JSON 통합 검색` 체크박스 | `doc search --query <검색어> [--doc <문서키>] [--json]` |
| 항목 조회 | 좌측 목록 클릭 시 우측 동적 폼에 파싱 출력 | `doc get --doc <문서키> [--id <레코드ID>] [--json]` |
| 항목 필드 편집 및 저장 | 우측 폼(문자/숫자/불리언/중첩 JSON) 수정 후 `변경사항 저장` | `doc set --doc <문서키> --id <레코드ID> (--field <필드> --value <값> \| --patch-json '<JSON>') [--json]` |
| 신규 항목 추가 / 복제 | `+ 신규 추가` / `복제` 버튼 | `doc add --doc <문서키> --json-data '<JSON>' [--json]` |
| 항목 삭제 | `삭제` 버튼 | `doc delete --doc <문서키> --id <레코드ID> [--json]` |
| 전체 기획 JSON 교차 참조 검증 | `전체 데이터 무결성 검증` 버튼 | `doc validate [--json]` |

### CLI 예시
```powershell
python tools/designer_tool.py doc list --json
python tools/designer_tool.py doc search --query "슬라임" --json
python tools/designer_tool.py doc get --doc items --id sword_steel --json
python tools/designer_tool.py doc set --doc items --id sword_steel --field price --value 195 --json
python tools/designer_tool.py doc set --doc monsters --id slime --patch-json '{"max_hp": 55, "gold_reward": 22}' --json
python tools/designer_tool.py doc validate --json
```

---

## 5. 로비 화면 편집 (`lobby`)

저장 파일: `data/ui_layout.json` (`lobby` 섹션, 기준 해상도 `1280×720`)  
C++ 게임의 `UIManager::RenderTitleScreen()`이 이 데이터를 직접 읽어 배경 그라데이션과 모든 요소(`sprite`, `text`, `guide_card`, `button`, `panel`)를 동적으로 렌더링합니다.

### 구현된 기능 목록 (UI 탭 5 + CLI)
| 기능 | UI 동작 | CLI 명령어 |
| :--- | :--- | :--- |
| 로비 레이아웃 조회 | 1280×720 축소 WYSIWYG 캔버스 + 하단 요소 목록 | `lobby show [--json]` |
| 배경 그라데이션 색상 편집 | 상단/하단 RGBA 입력 및 색상 선택기 | `lobby set-bg [--top R,G,B,A] [--bottom R,G,B,A] [--json]` |
| 요소 위치/크기/스타일/텍스트/아트 편집 | 캔버스 드래그 이동·우하단 핸들 리사이즈(8px 스냅) + 우측 인스펙터 | `lobby set --id <id> [--x N] [--y N] [--width N] [--height N] [--visible true\|false] [--art-id N] [--font-size N] [--text-key K] [--text T] [--align left\|center\|right] [--color RGBA] [--bg-color RGBA] [--hover-color RGBA] [--border-color RGBA] [--patch-json '<JSON>'] [--json]` |
| 커스텀 요소 추가 | `+ 텍스트 추가` / `+ 스프라이트 추가` / `+ 패널 추가` | `lobby add --json-data '<JSON>' [--json]` |
| 요소 삭제 | `선택 요소 삭제` | `lobby delete --id <id> [--json]` |
| 기본 레이아웃 복원 | `기본값 복원` | `lobby reset [--json]` |

### CLI 예시
```powershell
python tools/designer_tool.py lobby show --json
python tools/designer_tool.py lobby set-bg --top 14,20,38,255 --bottom 48,18,28,255 --json
python tools/designer_tool.py lobby set --id start_button --y 600 --width 300 --bg-color 40,115,65,255 --json
python tools/designer_tool.py lobby add --json-data '{"id":"version_tag","type":"text","x":1080,"y":688,"width":180,"height":24,"text":"v1.1","font_size":14,"align":"right"}' --json
```

---

## 6. 인플레이 UI (HUD & 팝업창) 편집 (`ui-hud`)

저장 파일: `data/ui_layout.json` (`hud.widgets` 섹션, 기준 해상도 `1280×720`)  
편집 가능 위젯 ID (`11종`):
* **상시 HUD (`group: hud`)**: `player_status`, `map_banner`, `boss_bar`, `quest_tracker`, `minimap`, `action_bar`, `toast_area`
* **팝업창 (`group: popup`)**: `dialogue_panel`, `shop_panel`, `inventory_panel`, `quest_log_panel`

### 구현된 기능 목록 (UI 탭 6 + CLI)
| 기능 | UI 동작 | CLI 명령어 |
| :--- | :--- | :--- |
| 인플레이 UI 위젯 목록 조회 | 1280×720 축소 WYSIWYG 캔버스 (`팝업창 함께 표시` 토글) + 위젯 목록 | `ui-hud list [--json]` |
| 단일 위젯 상세 조회 | 위젯 클릭 시 우측 인스펙터 표시 | `ui-hud get --id <widget_id> [--json]` |
| 위젯 위치/크기/표시여부/색상/미니맵 셀 편집 | 캔버스 드래그 이동·우하단 핸들 리사이즈(8px 스냅) + 우측 인스펙터 | `ui-hud set --id <widget_id> [--x N] [--y N] [--width N] [--height N] [--visible true\|false] [--cell-size N] [--bg-color RGBA] [--border-color RGBA] [--accent-color RGBA] [--patch-json '<JSON>'] [--json]` |
| 기본 레이아웃 복원 | `기본값 복원` | `ui-hud reset [--json]` |
| UI 레이아웃 무결성 검증 | `전체 데이터 무결성 검증`에 포함 | `ui-hud validate [--json]` |

### CLI 예시
```powershell
python tools/designer_tool.py ui-hud list --json
python tools/designer_tool.py ui-hud set --id minimap --x 14 --y 570 --cell-size 6 --json
python tools/designer_tool.py ui-hud set --id quest_tracker --visible false --json
python tools/designer_tool.py ui-hud validate --json
```
