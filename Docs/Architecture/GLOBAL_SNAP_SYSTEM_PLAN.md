# Global Snap Sistemi — Uygulama Planı

**Durum:** Adım 0–2 uygulandı, adım 3'ün bir kısmı yapıldı (aşağıdaki tablo). Derleme ve 53/53 otomasyon testi geçiyor; PIE ile denenmedi.
**Dal:** `feature/global-snap-kernel` (`sotf-wood-core-integration` üzerinden).

### İlerleme

| Adım | Durum | Not |
|---|---|---|
| 0 | Yapıldı | `CommitHeldLogAt` artık hep-ya-hiç: birincil destek, ikinci destek ve "bu parçaya yaslanan" parça (payandadaki kiriş) tek işlemde; biri başarısızsa kütük elde kalır. Test: `Placement.TwoSupportsOrNothing`. |
| 1 | Yapıldı | `Source/MyProject3/Placement/`: tipler, sağlayıcı arayüzü + kayıt, profil kataloğu (`Config/Placement/PlacementProfiles.json`) + doğrulama, açı ızgarası ve titreme önleme, `FPlacementTransaction`, `UPlacementKernelSubsystem`, kütük sağlayıcısı (75 cm modüler özellikler dahil). Sunum katmanı henüz ayrı dosya değil; HUD mevcut önizlemeyi çiziyor. |
| 2 | Yapıldı | Oyuncunun uç-uca / T snap'i çekirdekten geliyor (`FindBestSnapCandidate` eski döngüsü silindi). E basınca aynı çözüm transaction ile commit ediliyor; uzak ucun desteği de bu işlemin parçası. Snap artık tutulan kütüğün ölçeğini koruyor (eskiden hedefin ölçeğini alıyordu). |
| 3 | Kısmen | Payanda ve iki destekli tahta hep-ya-hiç commit kullanıyor. Çözüm hesabı (`SpanTwo`) hâlâ eski fonksiyonlarda. |
| 4–10 | Bekliyor | |
**İlgili:** `Config/Placement/SnapVariationLibrary.json` (PR #34, taslak katalog), `Docs/Architecture/GLOBAL_PLACEMENT_SNAP_VARIATION_LIBRARY.md`.

## 1. Amaç

Bugün kütük yerleştirme ~10 ayrı algoritmadan oluşuyor (snap, istif, payanda, direk aralığı, kirişi direğe kaldırma, zemin döşeme, basamak, üçgen dolgu, kapı, yardımcının plan doldurması). Her yeni özellik kendi kodunu getirdi.

Hedef: **tek bir yerleştirme çekirdeği.** Yeni bir malzeme (taş, çubuk, yeni kesit) eklemek yalnızca şunu gerektirsin:

- bir **özellik sağlayıcısı** (küçük C++ sınıfı: "uçlarım, istasyonlarım, yüzeylerim bunlar"),
- birkaç **profil** (JSON: "şu özellik şu özelliğe, şu açılarla, şu kuralla oturur").

Çekirdeğin kendisine dokunulmaz.

## 2. İki katman: hesap ve görünüm

| Katman | Ne yapar | Ne yapmaz |
|---|---|---|
| **Çözücü (arka plan)** | Açı ızgarası (15°), 75 cm istasyonlar, profil kuralları, çakışma, destek, kapasite. Tek bir `FPlacementSolution` üretir. | Hiçbir şey çizmez. |
| **Sunum (oyuncunun gördüğü)** | Hayaleti çözümün transformunda çizer; beyaz = oturur, kırmızı = oturmaz; bağlantı noktasında küçük işaret; kısa yazı ("E: yerleştir"). | Hiçbir şey hesaplamaz. Açı, derece, menü göstermez. |

İki katman arasında yalnızca `FPlacementSolution` geçer. **Önizlenen çözüm = commit edilen çözüm.**

Oyuncu nişan alır ve gerekirse tekerlekle kabaca döndürür. Çözücü bu niyeti en yakın izinli açıya ve istasyona yuvarlar (oyuncu 38° tuttu → 45°).

## 3. Çekirdek döngü

```
Solve(Intent):
  1. Aday parçaları bul        yakın, sınırlı yarıçap (oyuncunun bakış noktası çevresi)
  2. Özellikleri iste           her parçanın sağlayıcısından: uç, istasyon, yüzey, kenar
  3. Profil eşle                (kaynak özellik, hedef özellik) çiftine uyan profiller
  4. Çöz                        profilin çözücü tipiyle transform + bağlantılar
  5. Doğrula                    sert kurallar: başarısız aday puanlanmaz
  6. Puanla                     yalnız geçerli adaylar; titreme önleme
  7. Çözüm döndür               FPlacementSolution (geçerli ya da sebebiyle geçersiz)

Commit(Solution):
  1. Çözüm hâlâ güncel mi?      parça revizyonları önizlemedekiyle aynı mı
  2. Tüm bağlantıları ayır      hepsi ya da hiçbiri
  3. Uygula                     transform, yaşam döngüsü, sahiplik, grafik kenarları, attach
  4. Hata olursa                ters sırayla geri al, durum "Placed" olmaz
  5. Fizik                      yalnız profilin fizik politikası istiyorsa
```

## 4. Temel kavramlar

### 4.1. Özellik (feature)

Bir parçanın bağlantıya katılabilen yeri. Aktör veya bileşen **değildir**; sağlayıcı istendiğinde üretir.

| Tür | Örnek | Not |
|---|---|---|
| `End` | Kütüğün iki kesit yüzü | Gerçek geometriden, her zaman 2 tane |
| `Station` | Kütük gövdesinde 75 cm modülün ortası | Yönü yok; yön çözümde hesaplanır |
| `Face` | Taşın düz yüzü, kalasın üst yüzü | Normal ve sınırlarıyla |
| `Edge` | Taş kenarı | |
| `Authored` | Mesh'te elle tanımlı özel nokta (çentik, yuva) | Otomatik üretim bunu silmez |
| `Surface` | Zemin, arazi | Sağlayıcı yok; çarpışma sorgusundan |

### 4.2. Kararlı kimlik

Bağlantılar dizi sırasına değil kimliğe bağlanır:

```
FPlacementFeatureRef = { PieceId (FGuid), FeatureId (FName), GeometryRevision (int32) }
```

- `FeatureId` örnekleri: `End.A`, `End.B`, `Station.03`, `Face.Top`.
- Kesim veya mesh değişince `GeometryRevision` artar; eski önizleme geçersiz olur.

### 4.3. Profil

Veri dosyasındaki bir kural: hangi özellik hangi özelliğe, hangi çözücüyle, hangi açılarla, kaç bağlantıyla oturur.

### 4.4. Çözüm

```cpp
struct FPlacementSolution
{
    FName ProfileId;
    FTransform PieceTransform;                    // parçanın son yeri
    TArray<FPlacementLink> Links;                 // her biri: kaynak ref + hedef ref + rol (Primary/Secondary)
    float TrimLengthCm = 0;                       // payanda gibi profiller parçayı kısaltabilir
    bool bValid = false;
    FText Reason;                                 // geçersizse neden (oyuncuya kısa yazı)
    uint32 SolveStamp;                            // commit'te güncellik kontrolü
};
```

## 5. Kütük özellik sağlayıcısı

Kurallar:

- **Uçlar:** her zaman 2, gerçek uç düzlemlerinden.
- **İstasyonlar:** `N = floor(L / 75)`, konum `x_i = 37.5 + 75·i` (başlangıç ucundan).
  - 600 → 8, 450 → 6, 300 → 4, 150 → 2, 75 → 1.
  - 75'e bölünmeyen artık (ör. 400 cm'de son 25 cm) istasyon almaz.
- **`L` dünya uzunluğudur:** mesh sınırı × aktör ölçeği. Serbest kesim (`TrimLogToRange`) parçayı aktör ölçeğiyle kısalttığı için ölçek **reddedilmez**, hesaba katılır.
- **Dosya adından uzunluk okunmaz.**
- **Kesit:** yuvarlak kesitte istasyonun yönü serbesttir (çözüm sırasında hesaplanır). Kalas, yarım, kare kirişte yalnız düz yüzler (`Face.*`) geçerlidir.
- **Yön kopyası yok:** bugünkü `AutoSide_X_Plus_00` gibi 4 yönlü noktalar kaldırılır.

Bugünkü `FLogSnapPoint` dizisi geçiş süresince kalır. Sağlayıcı gerektiğinde ondan okur, ama yeni kod ona yazmaz.

## 6. Diğer malzemeler: mevcut kod yeniden kullanılır

Projede zaten ortak parçalar var:

| Mevcut | Kullanımı |
|---|---|
| `IStructuralPieceInterface` (kütük, taş, çubuk, ip) | Sağlayıcı arayüzünün tohumu: `GetStructuralAnchorIds`, `GetStructuralAnchor`, `SetStructuralAnchorOccupied` |
| `UMaterialConnectionSubsystem` | Kural tablosu ve "grafik + ayırma + attach ya hep ya hiç" commit'i. Transaction'ın ilk sürümü buradan genişletilir. |
| `FStoneSnapAnchor`, `FPrimitiveAnchor` | Taş ve çubuk sağlayıcıları bunları `Authored` özellik olarak sunar |
| `UWorldStructuralGraphSubsystem` | Yapısal bağlantılar: `TryConnect`, `TryAddSecondarySupport`, `TryRemoveSecondarySupport` |

Yani taş ve çubuk için ilk sürümde yeni geometri yazılmaz. Mevcut ankrajlar sağlayıcı üzerinden çekirdeğe açılır.

## 7. Çözücü tipleri

Profil sayısı artar, çözücü sayısı artmaz.

| Çözücü | Ne çözer | Kullanan profiller |
|---|---|---|
| `AlignOne` | Tek kaynak özelliği tek hedefe hizalar | uç-uca, uç-gövde (T), taş yüz-yüz, çubuk ucu |
| `AlignParallel` | Kaynağı hedefe paralel yatırır, istasyon ızgarasına oturtur | duvar istifi, zemin döşeme |
| `SpanTwo` | Kaynağın iki ucunu iki ayrı hedefe birlikte çözer, gerekirse uzunluk hesaplar | iki destekli kiriş, payanda, merdiven |
| `RestOnSurface` | Yüzeye oturtur, yapısal bağlantı kurmaz | zemine bırakma, direk dikme, ateş yeri |

Yeni bir davranış önce mevcut dört çözücüden biriyle ifade edilmeye çalışılır. Yeni çözücü ancak hiçbiri uymuyorsa eklenir.

## 8. Profiller ve açılar

### 8.1. Açı politikası (profil başına)

| Politika | Anlam |
|---|---|
| `Fixed` | Tek açı (ör. uç-uca: 0°) |
| `Grid` | İzinli açı listesi; niyet en yakına yuvarlanır |
| `Range` | Aralık içinde serbest, adım ile yuvarlanır |

Varsayılan ızgara 15°: `0, 15, 30, 45, 60, 75, 90`. Açı setleri JSON'dadır; değiştirmek kod değişikliği değildir.

### 8.2. İlk profil seti (bugünkü özelliklerin karşılığı)

| Profil | Bugünkü kod | Çözücü | Açılar | Bağlantı |
|---|---|---|---|---|
| `log.end_to_end` | `FindBestSnapCandidate` | AlignOne | Fixed 0° | 1 |
| `log.end_to_station` (T) | `FindBestSnapCandidate` | AlignOne | Grid 90 (+15'lik sapmalar ileride) | 1 |
| `log.station_cross` | `FindBestSnapCandidate` | AlignOne | Grid 15° | 1 |
| `log.parallel_stack` | `UpdateStackPlacement` | AlignParallel | Fixed 0° | 1–2 |
| `log.beam_two_support` | `CommitHeldLogAt` + `ApplyEndContactSupports` | SpanTwo | Fixed (destekler belirler) | **2 zorunlu** |
| `log.strut` | `UpdateStrutPlacement` / `PlaceStrut` | SpanTwo | Grid 30/45/60 | 2 zorunlu |
| `log.post_on_ground` | `PlaceHeldPiece` (dikey) | RestOnSurface | Fixed 90° dikey | 0 (zemin) |
| `log.ground_rest` | `DropHeldPiece` | RestOnSurface | serbest | 0 |
| `log.deck` | `TryPlaceDeck` | AlignParallel | Fixed 90° kirişlere | 2 |
| `log.step` | `TryPlaceSingleStep` | AlignParallel | Fixed | 1 |
| `log.apex_fill` | `TryFillApex` | AlignParallel (tekrarlı) | Fixed | 1–2 |
| `log.hinge_swing` | menteşe/sarkma yolu | AlignOne + fizik | — | 1, **yalnız açıkça seçilirse** |
| `stone.face_to_face` | `AStoneBuildingPiece::TryConnectToAnchor` | AlignOne | Grid 90 | 1 |
| `stone.on_log_station` | `UMaterialConnectionSubsystem` | AlignOne | Grid 15° | 1 |
| `stick.end_to_end` / `stick.cross` / `stick.on_log` | `AStickPrimitivePiece::TryConnectToAnchor` | AlignOne | Grid 15° | 1 |

Direk aralığı (`SnapPostSpacing`) ayrı kod olmaktan çıkar: 75 cm modül kuralı ve `SpanTwo` bunu kendiliğinden verir.

### 8.3. Profil alanları (JSON)

```
id, operation (Connect | SurfacePlace | Receiver | Voxel | Process),
source { feature, tags }, target { feature, tags, family },
solver, angle { policy, values | min/max/step, toleranceDeg },
positionToleranceCm, requiredLinks, capacity (hedef özellik kaç bağlantı alır),
structural (Support | None), physics (Static | Rest | Hinge),
priority
```

Şema doğrulaması: `requiredLinks: 2` olan profil `SpanTwo` çözücüsü olmadan geçersizdir, vb.

## 9. Doğrulama ve puanlama

**Sert kurallar (başarısızsa aday elenir, puanlanmaz):**

- Profil uyumsuz (malzeme, kesit, rol).
- Hedef özelliğin kapasitesi dolu veya başka işlem ayırmış.
- Gerekli bağlantıların hepsi bulunamadı (iki destekli kiriş tek destek bulursa → geçersiz, tek desteğe **düşmez**).
- Çakışma.
- Hasarlı hedef (`Damage ≥ %50 eşik`).
- Yapı kuralı: açıklık 400 cm üstü, çıkma 220 cm üstü, 7. duvar sırası, ayrık parça direk olamaz.

**Puan (yalnız geçerli adaylar):**

- nişan noktasına uzaklık,
- oyuncunun tuttuğu açıya fark,
- profil önceliği.

**Titreme önleme:** önceki karedeki çözüm, yeni aday belirgin şekilde daha iyi olmadıkça (eşik) korunur.

## 10. Commit ve geri alma (transaction)

Sıra:

1. `SolveStamp` ve parça `GeometryRevision` kontrolü. Değiştiyse yeniden çöz, commit etme.
2. Tüm hedef özellikleri ayır (`SetStructuralAnchorOccupied`). Biri başarısızsa öncekileri bırak, çık.
3. Transform uygula.
4. Yaşam döngüsü `Held → Placed`.
5. Sahiplik `Held → World`.
6. Grafik: birincil `TryConnect`, sonra her ikincil `TryAddSecondarySupport`. **Sonuçları kontrol edilir.**
7. Attach.
8. Herhangi bir adım başarısız → ters sırayla geri al, parça elde kalır, oyuncuya sebep gösterilir.

Bugün `CommitHeldLogAt` içinde ikinci desteğin sonucu kontrol edilmiyor (`LogCarryComponent.cpp:3209`). Bu, yeni sistemin ilk kabul testidir.

Yardımcı karakter de aynı `Commit`'i çağırır. Oyuncuyla aynı özelliği hedeflerse ikinci gelen reddedilir.

## 11. Fizik politikası

| Politika | Sonuç |
|---|---|
| `Static` | Yerleşir, fizik kapalı, yapısal bağlantı var |
| `Rest` | Fizik açık, düşer/oturur, yapısal bağlantı yok |
| `Hinge` | Tek uç bağlı, serbest uç sallanır; yalnız açık profil |

Başarısız bir kiriş hiçbir zaman `Hinge`'e veya `Rest`'e kendiliğinden dönüşmez.

## 12. Kesim ile ilişki

- Kesim ayrı bir işlemdir (`Process`), snap profili değildir.
- Kesim tamamlanınca yeni parçalar oluşur, her biri kendi uç ve istasyonlarını gerçek uzunluğundan üretir, `GeometryRevision` artar.
- 25 cm serbest kesim aynen kalır. 75 cm istasyon kuralı kesim adımını değiştirmez.
- Kesim yarıdayken parça topolojisi değişmez.

## 13. Geçiş sırası

Her adımda **mevcut 48 otomasyon testi geçmeli**. Yeni yol eski davranışı verdiği anda eski kod silinir; iki karar verici birlikte kalmaz.

| Adım | İş | Silinecek / sarmalanacak eski kod |
|---|---|---|
| 0 | Hata düzeltmesi: ikinci destek başarısızsa yerleştirme geri alınır + test | — |
| 1 | Çekirdek tipler, profil yükleyici + şema, kütük sağlayıcısı, `AlignOne`, sunum katmanı | — |
| 2 | Uç-uca, T, kesişme çekirdeğe | `FindBestSnapCandidate` aday arama kısmı, `SetSnapCandidate` indeksleri |
| 3 | Transaction + `SpanTwo`: iki destekli kiriş, payanda | `CommitHeldLogAt` destek kısmı, `UpdateStrutPlacement`, `PlaceStrut` |
| 4 | `AlignParallel`: istif, döşeme, basamak, üçgen dolgu | `UpdateStackPlacement`, `TryPlaceDeck`, `TryPlaceSingleStep`, `TryFillApex` |
| 5 | `RestOnSurface`: direk dikme, zemine bırakma | `PlaceHeldPiece` dikey yolu, `SnapPostSpacing` |
| 6 | Plan slotları ve yardımcı karakter çekirdeği kullanır | `TryFillPlanSlot`, `HelperCompanionActor::PlaceInPlan` içindeki doğrudan yerleştirme |
| 7 | Kirişi direğe kaldırma / sarkan ucu oturtma, çekirdeğin "destek değişti → yeniden çöz" olayına bağlanır | `RaiseBeamsOntoPost`, `LiftDroopingEndsOnto`, `CommitHeldLifts` |
| 8 | 150 → 75 cm istasyonlara geçiş; plan slotları ve testler güncellenir | `GenerateDefaultSideSnapPoints` 4 yönlü noktaları |
| 9 | Taş ve çubuk sağlayıcıları + profilleri (modülerliğin kanıtı: çekirdek değişmemeli) | taş/çubuk içindeki kendi aday arama kodları |
| 10 | Kayıt/yükleme: bağlantı kimlikleri, grafik kenarları, revizyonlar | — |
| sonra | Ateş yeri (`RestOnSurface` + Receiver), voxel (ayrı `Voxel` işlemi) | — |

Adım 1–7 boyunca istasyon aralığı **150 cm uyumluluk ayarıyla** çalışır. Yeni sistem önce eski davranışı birebir verir. 75 cm'e geçiş adım 8'de ayrı yapılır.

## 14. Testler

**Sağlayıcı:**

- 600/450/300/150/75 cm → 8/6/4/2/1 istasyon + 2 uç.
- Ölçekle kısaltılmış parça: istasyonlar dünya uzunluğuna göre.
- Kesimden sonra eski `FeatureId`'ler yeni parçaya taşınmaz.

**Çözücü:**

- 38° tutulan kesişme 45°'e oturur.
- İki açı arasında titreme yok.
- Kalas yuvarlak gibi her yönden bağlanmaz.

**Transaction:**

- İkinci destek yoksa kiriş yerleşmez.
- İkinci destek grafikte reddedilirse her şey geri alınır.
- Önizleme ile E arasında hedef kaldırıldı → commit reddedilir.
- Oyuncu ve yardımcı aynı özelliği hedefler → biri reddedilir.
- Başarısızlıkta ayrılmış özellik kalmaz.

**Modülerlik:**

- Taş sağlayıcısı eklemek çekirdek dosyalarında değişiklik gerektirmez (diff kontrolü).

**Unreal:**

- Derleme, otomasyon, PIE ayrı ayrı raporlanır.

## 15. Dosyalar (planlanan)

```
Source/MyProject3/Placement/
  PlacementTypes.h              FPlacementFeature, FPlacementFeatureRef, FPlacementIntent, FPlacementSolution, FPlacementLink
  PlacementFeatureProvider.h    IPlacementFeatureProvider
  PlacementProfileRegistry.h/.cpp   JSON yükleme + doğrulama
  PlacementSolvers.h/.cpp       AlignOne, AlignParallel, SpanTwo, RestOnSurface (düz fonksiyonlar)
  PlacementKernelSubsystem.h/.cpp   Solve, Commit, rezervasyon, geri alma
  PlacementPreview.h/.cpp       yalnız çizim
  LogFeatureProvider.cpp        ALogBuildPiece için
  StoneFeatureProvider.cpp      (adım 9)
  StickFeatureProvider.cpp      (adım 9)
Config/Placement/SnapVariationLibrary.json   PR #34'ten alınır, bu plana göre yeniden düzenlenir
```

Puanlayıcı, aday üretici gibi parçalar ayrı UObject değildir; alt sistemin içinde düz fonksiyonlardır.

## 16. Açık kararlar

1. **75 cm geçişi:** adım 8'de mi, yoksa baştan mı? Öneri: adım 8.
2. **Açı setleri:** profil başına önerilen değerler tablo 8.2'de. Oyun hissine göre değişebilir.
3. **Varlıklar:** `Long_01–08` / `Short_01–04` (hepsi 75 cm) gözle kontrol edilip biri `EighthLength + Round` olarak atanabilir mi?
4. **PR #34:** bu plana göre düzenlenip bu dala alınsın mı, yoksa kapatılsın mı?

## 17. Tamamlandı sayılma koşulu

- Tablo 13'teki eski fonksiyonlar silinmiş ya da yalnız çekirdeği çağıran ince sarmalayıcı.
- Oyuncu, yardımcı ve plan slotları aynı `Commit`'i kullanıyor.
- İki destekli kiriş ve payanda: ya tam yerleşir ya hiç.
- Taş ve çubuk çekirdek değişmeden eklenmiş.
- 48 eski test + yeni testler geçiyor; PIE kontrol listesi yapılmış.
