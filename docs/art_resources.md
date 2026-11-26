# 아트 리소스 명세서 (Art Resource Specification)

본 문서는 게임에 필요한 모든 2D 아트 리소스의 **고유 번호(Art ID)**, **저장 경로**, **픽셀 규격**, **스프라이트 프레임 구조**, **아트 컨셉**을 정의한다.  
게임 엔진(`ResourceManager`)은 본 문서를 바탕으로 생성된 번호-경로 전용 매핑 파일 [`data/art_mapping.txt`](../data/art_mapping.txt)만 읽어 모든 텍스처를 메모리에 로드한다.

---

## 1. 번호 대역 규칙
| 번호 대역 | 카테고리 | 저장 디렉토리 |
| :---: | :--- | :--- |
| `1 ~ 99` | 지형 타일 (Tiles) | `assets/tiles/` |
| `101 ~ 199` | 플레이어 8방향 스프라이트 시트 | `assets/characters/` |
| `201 ~ 299` | 필드 몬스터 스프라이트 시트 | `assets/monsters/` |
| `301 ~ 399` | 보스 몬스터 스프라이트 시트 | `assets/boss/` |
| `401 ~ 499` | NPC 스프라이트 시트 | `assets/npcs/` |
| `501 ~ 599` | 아이템 아이콘 (UI/인벤토리/상점) | `assets/items/` |
| `601 ~ 699` | 전투 이펙트 / 투사체 / UI 보조 | `assets/effects/` |

---

## 2. 플레이어 8방향 스프라이트 시트 규격 (`101 ~ 103`)
- **시트 크기**: `192 × 384` px (프레임 `48 × 48` px, **4열(프레임) × 8행(방향)**)
- **행(Row) 인덱스 = `Direction8`**:
  - `Row 0`: 하 (South, ↓)
  - `Row 1`: 좌하 (South-West, ↙)
  - `Row 2`: 좌 (West, ←)
  - `Row 3`: 좌상 (North-West, ↖)
  - `Row 4`: 상 (North, ↑)
  - `Row 5`: 우상 (North-East, ↗)
  - `Row 6`: 우 (East, →)
  - `Row 7`: 우하 (South-East, ↘)
- **캐릭터 외형 컨셉**: 은빛 판금 투구·푸른 망토·은검과 방패를 장비한 중세 방랑 기사. 방향에 따라 얼굴 바이저, 망토의 펄럭임, 방패와 검의 좌우 위치 및 검격 호(Arc)가 정확히 8방향으로 회전 표현된다.

---

## 3. 전체 아트 리소스 목록

### 3.1 지형 타일 (`1 ~ 16`) — 단일 이미지 `32 × 32` px
| Art ID | 경로 | 규격 | 아트 컨셉 |
| :---: | :--- | :---: | :--- |
| `1` | `assets/tiles/tile_00_grass.png` | 32×32 | 짙은 녹색 초원 풀밭 + 풀잎 질감 |
| `2` | `assets/tiles/tile_01_dirt_path.png` | 32×32 | 황토빛 흙길 + 자갈 디테일 |
| `3` | `assets/tiles/tile_02_cobblestone.png` | 32×32 | 마을 회베이지 보도블록 석재 바닥 |
| `4` | `assets/tiles/tile_03_dungeon_floor.png` | 32×32 | 성채 청회색 석판 + 균열 |
| `5` | `assets/tiles/tile_04_red_carpet.png` | 32×32 | 금장 테두리 진홍색 왕실 카펫 |
| `6` | `assets/tiles/tile_05_bridge.png` | 32×32 | 강 위 갈색 목재 다리 판자 + 난간 |
| `7` | `assets/tiles/tile_06_tree.png` | 32×32 | 풍성한 고목 수관 + 나무줄기 |
| `8` | `assets/tiles/tile_07_house_wall.png` | 32×32 | 목재 프레임 석조 건물 벽 + 격자 창문 |
| `9` | `assets/tiles/tile_08_house_roof.png` | 32×32 | 붉은 테라코타 기와 지붕 |
| `10` | `assets/tiles/tile_09_dungeon_wall.png` | 32×32 | 흑요석 성벽 블록 + 상단 하이라이트 |
| `11` | `assets/tiles/tile_10_water.png` | 32×32 | 청록색 강물 + 흰색 잔물결 |
| `12` | `assets/tiles/tile_11_lava.png` | 32×32 | 붉고 주황빛으로 이글거리는 용암 |
| `13` | `assets/tiles/tile_12_flower_grass.png` | 32×32 | 야생화가 핀 초원 |
| `14` | `assets/tiles/tile_13_fountain.png` | 32×32 | 푸른 물줄기가 솟는 마을 석조 분수 |
| `15` | `assets/tiles/tile_14_portal_field.png` | 32×32 | 푸른 아케인 마법진 이동 포탈 |
| `16` | `assets/tiles/tile_15_portal_boss.png` | 32×32 | 진홍색 룬 보스 차원문 포탈 |

### 3.2 플레이어 스프라이트 (`101 ~ 103`) — `192 × 384` px (48×48 × 4×8)
| Art ID | 경로 | 규격 | 아트 컨셉 |
| :---: | :--- | :---: | :--- |
| `101` | `assets/characters/player_idle.png` | 192×384 | 기사 8방향 대기 호흡 애니메이션 (4프레임) |
| `102` | `assets/characters/player_walk.png` | 192×384 | 기사 8방향 보행(발·팔·망토 교차) 애니메이션 (4프레임) |
| `103` | `assets/characters/player_attack.png` | 192×384 | 기사 8방향 검격 휘두르기 + 검기 궤적 (4프레임) |

### 3.3 필드 몬스터 (`201 ~ 203`) — `160 × 40` px (40×40 × 4프레임 횡배열)
| Art ID | 경로 | 규격 | 아트 컨셉 |
| :---: | :--- | :---: | :--- |
| `201` | `assets/monsters/slime.png` | 160×40 | 잿빛 반투명 젤리 슬라임, 붉은 눈, 탄성 바운스 4프레임 |
| `202` | `assets/monsters/goblin.png` | 160×40 | 녹색 피부·뾰족귀·단검을 든 고블린 약탈자 이동/공격 4프레임 |
| `203` | `assets/monsters/skeleton.png` | 160×40 | 녹슨 방패와 도끼를 든 백골 파수꾼 보행 4프레임 |

### 3.4 보스 몬스터 (`301`) — `320 × 160` px (80×80 × 4열 × 2행)
| Art ID | 경로 | 규격 | 아트 컨셉 |
| :---: | :--- | :---: | :--- |
| `301` | `assets/boss/ashen_lord.png` | 320×160 | 잿빛 군주 말라코르 — 뿔 달린 왕관, 검은 갑주, 화염 대검. 1행: 기본(1페이즈), 2행: 화염 오라 광폭화(2페이즈) |

### 3.5 NPC (`401 ~ 403`) — `160 × 40` px (40×40 × 4프레임)
| Art ID | 경로 | 규격 | 아트 컨셉 |
| :---: | :--- | :---: | :--- |
| `401` | `assets/npcs/npc_elder.png` | 160×40 | 흰 수염과 갈색 로브, 지팡이를 든 촌장 엘드릭 (대기 4프레임) |
| `402` | `assets/npcs/npc_merchant.png` | 160×40 | 붉은 앞치마와 금화 주머니를 찬 상인 마라 (대기 4프레임) |
| `403` | `assets/npcs/npc_healer.png` | 160×40 | 순백·하늘색 성녀 로브와 치유 지팡이를 든 세라 (대기 4프레임) |

### 3.6 아이템 아이콘 (`501 ~ 509`) — `32 × 32` px
| Art ID | 경로 | 규격 | 아트 컨셉 |
| :---: | :--- | :---: | :--- |
| `501` | `assets/items/item_potion_hp.png` | 32×32 | 붉은 액체가 담긴 둥근 유리 포션 병 |
| `502` | `assets/items/item_potion_hi.png` | 32×32 | 황금 장식 마개가 달린 대형 진홍 엘릭서 병 |
| `503` | `assets/items/item_sword_iron.png` | 32×32 | 기본 철제 단검 아이콘 |
| `504` | `assets/items/item_sword_steel.png` | 32×32 | 빛나는 강철 양날 장검 아이콘 |
| `505` | `assets/items/item_sword_runic.png` | 32×32 | 푸른 룬 문자가 발광하는 성검 아이콘 |
| `506` | `assets/items/item_armor_leather.png` | 32×32 | 갈색 강화 가죽 흉갑 아이콘 |
| `507` | `assets/items/item_armor_knight.png` | 32×32 | 은빛 기사 판금 흉갑 아이콘 |
| `508` | `assets/items/item_armor_ashen.png` | 32×32 | 검은 강철과 금장 룬이 새겨진 수호갑옷 아이콘 |
| `509` | `assets/items/item_ashen_crown.png` | 32×32 | 붉은 보석이 박힌 고대 잿빛 왕관 아이콘 |

### 3.7 이펙트 및 투사체 (`601 ~ 604`) — `32 × 32` px (또는 `128 × 32` 4프레임)
| Art ID | 경로 | 규격 | 아트 컨셉 |
| :---: | :--- | :---: | :--- |
| `601` | `assets/effects/fx_slash.png` | 128×32 | 흰색~하늘색 검기 참격 호 4프레임 |
| `602` | `assets/effects/fx_whirlwind.png` | 128×32 | 황금/청색 원형 회오리 베기 충격파 4프레임 |
| `603` | `assets/effects/fx_fireball.png` | 128×32 | 보스 화염 탄막 구슬 회전 4프레임 |
| `604` | `assets/effects/fx_coin.png` | 32×32 | 반짝이는 금화 UI 아이콘 |

### 3.8 툴로 추가 업로드된 아트 리소스
| Art ID | 경로 | 규격 | 아트 컨셉 |
| :---: | :--- | :---: | :--- |
| `204` | `assets/monsters/ember_wraith.png` | 160x40 | 검은 로브 속에서 붉은 잿불과 불꽃이 타오르는 다크 판타지 유령 몬스터 4프레임 부유 시트 |
| `510` | `assets/items/item_sword_ember.png` | 32x32 | 검은 흑요석 칼날 가장자리에서 주황빛 용암 불꽃이 타오르는 대검 아이콘 |
