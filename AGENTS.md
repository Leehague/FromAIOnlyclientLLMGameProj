# Project Context & Workspace Rules — Chronicle of the Ashen Crown

이 파일(`AGENTS.md`)은 Antigravity가 이 프로젝트의 **모든 프롬프트 요청**에서 자동으로 로드하는 상시 컨텍스트 및 작업 규칙입니다.

---

## 1. 프로젝트 개요 및 기술 환경
- **프로젝트**: 《잿빛 왕관의 연대기 (Chronicle of the Ashen Crown)》 — 클라이언트 온리 솔로 2D 탑뷰 타일맵 RPG
- **기술 스택**: C++17, `raylib 5.5`, `nlohmann/json 3.11.3`, CMake (Windows Visual Studio 2022 MSVC x64, `/utf-8`)
- **빌드 및 자동 검증**:
  - 빌드 & 스모크 테스트: `powershell -ExecutionPolicy Bypass -File build.ps1 -SmokeTest`
  - 실행 파일: `build\Release\AshenCrownRPG.exe` (검증 모드: `--smoke-test`)
- **GitHub 원격 저장소**: `https://github.com/Leehague/FromAIOnlyclientLLMGameProj.git` (`build/` 등 빌드 산출물은 `.gitignore`로 제외)

---

## 2. 아키텍처 및 데이터 드리븐 핵심 규칙
1. **싱글톤 통합 Manager 구조 유지**:
   - 전역 진입점은 단일 싱글톤 `GameManager`(`src/core/GameManager.h`) 하나만 유지한다.
   - 새로운 기능 매니저를 추가할 때는 반드시 `IManager`(`src/core/IManager.h`)를 상속받아 `GameManager` 내부에 소유·등록한다.
2. **하드코딩 금지 (100% 데이터 드리븐 원칙)**:
   - 모든 게임 스펙(플레이어·아이템·몬스터·보스·NPC·퀘스트·맵·타일)은 `data/*.json`에서 로드한다.
   - 모든 사용자 노출 UI 텍스트, 알림(Toast), 전투/NPC 문구, 방향 이름, 폰트 설정은 **`data/strings.json`**에서 관리한다. **C++ 소스 코드(`src/`)에 한글이나 UI 문구를 직접 하드코딩하지 않는다.**
   - `ResourceManager::LoadUIFont()`는 `data/*.json` 전체 텍스트와 `strings.json` 설정에서 한글/기호 글리프를 자동 수집하는 방식을 유지한다.
3. **맵 및 아트 리소스 분리 규칙**:
   - 타일맵 지형은 `data/maps/*.txt`(타일 번호 공백 구분 배열)에 저장하고, 타일 속성은 `data/tiles.json`과 `docs/tile_design.md`에 동기화한다.
   - 게임 로직은 `data/art_mapping.txt`(Art ID ↔ 상대 경로)만 읽어 텍스처를 로드하며, 에셋 규격은 `docs/art_resources.md`에 기록한다.
4. **몬스터/보스 AI 길찾기**:
   - 몬스터 및 보스 추적은 `MapManager::FindPath`의 8방향 타일 기반 A\*(코너 끼임 방지 + 대형 개체 여유 반경 + 주기적 재탐색)를 사용한다.
5. **빌드 & 검증**:
   - `[에이전트 모드]`에서 코드나 데이터를 수정한 경우, 가능하면 Release 빌드 및 `AshenCrownRPG.exe --smoke-test`로 정상 로딩과 동작을 검증한다.
6. **기획자용 툴 CLI 사용 시 참조**:
   - 맵 제작/수정·아트 업로드·아트 요구사항 조회/등록·기획 JSON 편집·로비/인플레이 UI 편집 등 미리 구현된 기획자용 툴을 CLI 기반으로 구동할 때만 [docs/designer_tool_cli.md](docs/designer_tool_cli.md)를 먼저 읽고 해당 명령을 사용한다.
7. **`[기획자 모드]` 동작 규칙 (기획자 툴 CLI 전용 제한)**:
   - 사용자가 **`[기획자 모드]`**를 명시한 경우, 작업 수단을 **미리 구현된 기획자용 툴 CLI(`python tools/designer_tool.py ...`)로만 제한**한다.
   - 시작 시 [docs/designer_tool_cli.md](docs/designer_tool_cli.md)를 읽고 해당 문서에 정의된 CLI 명령(`map`, `art`, `art-req`, `doc`, `lobby`, `ui-hud`, `validate`)만 실행한다.
   - **`write_file` / `edit_file` 등으로 C++ 소스(`src/`), 툴 코드(`tools/`), 빌드 설정, 또는 `data/`·`docs/` 파일을 직접 수정하지 않는다.** (모든 데이터 변경은 반드시 `designer_tool.py` CLI를 통해서만 수행한다.)
   - 요청한 작업이 현재 기획자 툴 CLI로 수행할 수 없는 기능이라면 임의로 파일을 직접 수정하지 말고, 미지원 사항임을 안내한 뒤 **`[에이전트 모드]` 전환 또는 툴 기능 추가** 여부를 사용자에게 먼저 확인한다.
   - 툴 작업 완료 후에는 `python tools/designer_tool.py validate --json`으로 데이터 무결성을 검증한다.
8. **아트 리소스 생성 워크플로우 및 필수 참조 문서**:
   - 대기 중인 아트 요구사항을 처리하거나 신규 아트 리소스를 생성할 때는 **반드시 아래 4개 문서를 순서대로 읽고 진행**한다:
     1. [data/art_requests.json](data/art_requests.json) (또는 `python tools/designer_tool.py art-req list --status pending --json`): 생성 대상 `pending` 항목의 `request_id`, `category`, `size`, `suggested_art_id`, `target_path`, `prompt`, `notes` 확인
     2. [docs/art_resources.md](docs/art_resources.md): 프로젝트 아트 컨셉(중세 다크 판타지 2D 픽셀아트), Art ID 번호 대역(`1~99` 타일 / `101~199` 플레이어 8방향 / `201~299` 몬스터 / `301~399` 보스 / `401~499` NPC / `501~599` 아이템 / `601~699` 이펙트), 스프라이트 시트 프레임 규격(`32×32`, `40×40×4`, `48×48×4×8`, `80×80×4×2` 등) 확인
     3. [data/art_mapping.txt](data/art_mapping.txt): 기존 등록된 Art ID 및 파일 경로 충돌 여부 확인
     4. [docs/designer_tool_cli.md](docs/designer_tool_cli.md): 업로드·완료 처리(`art upload --fulfill-request`) 및 기획 JSON `art_id` 연결(`doc set`) CLI 명령 확인
   - **생성 및 반영 절차**:
     1. `pending` 요구사항의 `prompt`·`size`·프레임 규격에 맞춰 투명 배경 PNG를 생성한다(프레임 정렬이 중요한 픽셀아트 시트는 `tools/generate_art.py`/Pillow 기반 생성 또는 이미지 생성 모델 출력 후 리사이즈·배경 투명화 후처리).
     2. `python tools/designer_tool.py art upload --file <PNG경로> --category <카테고리> --id <suggested_art_id> --name <파일명> --resize <size> --concept "<설명>" --fulfill-request <REQ-ID> --json`을 실행해 `assets/` 배치·`art_mapping.txt` 등록·`docs/art_resources.md` 기록·`REQ-ID`의 `fulfilled` 전환을 일괄 처리한다.
     3. 임시 `art_id`를 사용 중이던 기획 JSON(`monsters.json`, `items.json` 등)이 있다면 `designer_tool.py doc set`으로 새 `art_id`를 연결한다.
     4. `python tools/designer_tool.py validate --json` 및 `AshenCrownRPG.exe --smoke-test`로 누락 텍스처 0건과 정상 로딩을 검증한다.

---

## 3. 프롬프트 마무리 필수 체크리스트 (`README.md` · GitHub 동기화 규칙)
**매 프롬프트 요청을 마무리하기 전에 반드시 아래 항목을 순서대로 점검한다:**
1. **`README.md` 업데이트 필요성 확인**:
   - 이번 요청에서 **코드(아키텍처·매니저·구현 방식·빌드/테스트)**, **기획(맵·캐릭터·아이템·몬스터·보스·퀘스트·조작키)**, **아트(리소스 목록·규격·생성 파이프라인)** 중 하나라도 변경되었는지 확인한다.
   - 변경된 내용이 있다면 응답을 종료하기 전에 [README.md](README.md)의 해당 파트(`1. 코드(프로그래밍)`, `2. 기획`, `3. 아트`)를 최신 상태로 반드시 업데이트한다.
   - 단순 질문·조사·변경 없는 작업이거나, 파일 직접 수정이 제한된 **`[기획자 모드]`** / **`[조언 모드]`**에서는 `README.md`를 직접 수정하지 않는다.
2. **`AGENTS.md` 동기화**:
   - 프로젝트의 핵심 아키텍처, 주요 데이터 파일 구조, 빌드/실행 규칙이 변경되었다면 이 파일(`AGENTS.md`)도 함께 갱신해 컨텍스트가 낡지 않도록 유지한다.
3. **GitHub 커밋 및 푸시 (`origin/main`)**:
   - `[에이전트 모드]`(또는 `[기획자 모드]`에서 기획자 툴 CLI를 통한 데이터 변경 후 `validate --json` 통과 시)에서 파일 생성·수정·삭제가 있었고, 검증과 동기화가 완료되었다면:
     1. `git status`로 변경 파일을 확인한다 (`build/` 등 `.gitignore` 제외 대상이 포함되지 않았는지 점검).
     2. 변경 내역을 명확히 요약한 커밋 메시지로 스테이징 및 커밋한다 (`git add` → `git commit -m "..."`).
     3. 원격 저장소(`origin/main`)에 푸시한다 (`git push origin main`).
   - `[조언 모드]`이거나 파일 변경이 없는 경우, 빌드/검증이 실패한 미완성 상태인 경우, 또는 사용자가 커밋·푸시 보류를 명시한 경우에는 커밋·푸시하지 않는다.
