# Belge haritası

**Son kontrol:** 2026-10-30 · kod dalı `game/p00-adopt-group-a` · commit `6ca8e9c`.

Kod dalında 101 belge var (`Docs/` 83, `memory/` 18). Çoğu geçmiş çalışma oturumlarının raporu. Neyin neye yaradığı:

## A. Güncel ve okunması gerekenler

| Belge (kod dalında) | Türü | Ne zaman okunur |
|---|---|---|
| **Bu dal: `ELIMIZDE_NE_VAR.md`** | Gerçek durum | Her zaman, ilk |
| `Docs/GameDesign/MYPROJECT3_OYUN_PLANI_TR.md` | **Plan / istek** (ne yapmak istiyoruz) | Yeni bir şey tasarlarken |
| `memory/ROADMAP.md` | Görev listesi ve çalışma kuralları (tek dal, derlemeden yazma kuralları, toplu düzeltme adımları) | Kod yazmadan önce |
| `memory/CURRENT_TASK.md` | Yarım kalan iş (kaldığın yerden devam) | Bir oturum kesildiyse |
| `memory/DECISIONS.md` | Kalıcı kararlar ve nedenleri | Bir kararı sorgularken |
| `memory/CODE_INDEX.md` | Bütün C++ dosyaları, sınıflar, fonksiyonlar (otomatik üretilir) | Bir adı ararken (baştan sona okunmaz) |
| `CLAUDE.md` (kök) | Derleme/test komutları, modül haritası | Claude Code oturumunda |
| `Source/MyProject3/Placement/CLAUDE.md` | Yerleştirme çekirdeğinin kuralları | Kütük/snap işinde |

## B. Kısmen eskimiş (dikkatli oku)

| Belge | Neden |
|---|---|
| `memory/PLAN_STATUS.md` | Görev görev durum tablosu doğru, ama bazı satırlarda "unbuilt" yazıyor; artık her şey derleniyor ve test ediliyor. |
| `memory/CODEBASE.md` | Bulgular bölümü değerli; harita tablosu 2026-10-29'dan kalma. Örneğin "VerticalSlice_C simülasyonu çalıştırmıyor" artık doğru değil. |
| `memory/STATE.md` | Dal tablosu güncel (2026-10-30), alt kısımları eski. |
| `Docs/Demo/VERTICAL_SLICE_C.md`, `Docs/Demo/RAPOR_SOTF_KUTUK_SISTEMI.md` | Kütük sistemi ve C sahnesi için doğru; yeni sistemleri anlatmıyor. |
| `Docs/Architecture/GLOBAL_SNAP_SYSTEM_PLAN.md`, `TERRAIN_ARCHITECTURE_DECISION.md` | Kendi alanında geçerli mimari kararlar. |

## C. Tarih (geçmiş oturum raporları) — yeni iş için okunmaz

Hepsi `Docs/Architecture/` altında, kendi tarihlerinde doğruydu ama durumu artık yansıtmıyorlar:

- `PHASE_01_*` … `PHASE_14_*` (faz iskeletleri, kapanış ve kabul listeleri): eski 14 fazlık planın raporları. Şimdiki plan 0–12 fazlı ve farklı.
- `PASS_A_*` … `PASS_H_*`, `MECHANIC_DETAILING_*`, `CORE_00x_*`, `WOOD_00x_*`: kod incelemeleri ve sözleşme notları.
- `GROUP_A_CODE_CLOSURE`, `PHASE_02_14_INTEGRATION_CLOSEOUT`, `P6_FARMING_DATA_CODE_CLOSEOUT`, `P7_SHARED_STRUCTURE_GRAPH_CLOSEOUT`: ChatGPT oturumlarının kapanış notları.
- `MECHANIC_REGISTRY_119*`, `M001_M119_GAP_ANALYSIS...`, `Docs/Design/MECHANICS_119_MASTER`, `Docs/Design/CURRENT_GAP_STATUS`, `Docs/Testing/DEMO_ACCEPTANCE_MAP_M001_M119`: eski 119 mekanik listesi. Yerini oyun planı ve `PLAN_STATUS` aldı.
- `ARCHITECTURE_14_PHASE_ROADMAP`, `MASTER_IMPLEMENTATION_CHECKLIST_AND_FINAL_DEMO`, `BRANCH_INTEGRATION_AUDIT`, `LANDSCAPE_BUILDER_IMPLEMENTATION_PLAN`, `REFERENCE_GAME_AND_LANDSCAPE_GAP_STATUS`: eski planlar ve denetimler.
- `memory/sessions/*`: oturum günlükleri.

## D. Vizyon ve istekler nereye yazılır?

Tek yer: `Docs/GameDesign/MYPROJECT3_OYUN_PLANI_TR.md` (kod dalında). Sohbetlerde gelen yeni fikirler oraya eklenir; bu dala değil. Bu dal yalnızca "şu an ne var" sorusunu cevaplar.

## Öneri (kullanıcı onayı bekliyor)

C grubundaki tarih belgeleri kod dalından `Docs/Archive/` klasörüne taşınabilir. Böylece güncel belgeler ilk bakışta görünür. Hiçbir şey silinmez.
