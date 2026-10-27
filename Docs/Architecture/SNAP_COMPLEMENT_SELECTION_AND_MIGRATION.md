# Snap Sistemi — Seçilen Mantıklar ve Tamamlayıcı Bağlantılar

**Branch:** `feature/curated-snap-complements`  
**Durum:** Kaynak katmanı ve hedefli otomasyon testleri eklendi; Unreal 5.8 derleme/PIE doğrulaması bu değişiklikte çalıştırılmadı.

## Karar: bir snap sistemi, farklı bağlantı profilleri

Bu çalışma yeni bir kernel eklemez. `feature/global-snap-kernel` içindeki sağlayıcı tabanlı `UPlacementKernelSubsystem` tek çalışma zamanı çekirdeği olarak kabul edilir. PR #34'teki varyasyon kataloğunun iyi kuralları (kararlı feature kimlikleri, 75 cm istasyonlar, sert kısıtlar, açık fizik politikası ve atomik çoklu bağlantı) tasarım gereksinimidir; ikinci bir `UPlacementKernelSubsystem` olarak eklenmemelidir.

## Korunacak güçlü mantıklar

| Mantık | Karar | Neden |
|---|---|---|
| `log.end_to_end` | Koru | Kütük uzatma ve aynı eksende birleşme için temel bağlantı. |
| `log.end_to_station` | Koru | Uçtan gövdeye T bağlantısı; hedef yüzü nişan noktasından seçilir. |
| `log.parallel_stack` | Koru | 75 cm modülde istif ve duvar sıraları. |
| `log.deck` / `log.stairs` | Koru | Birden fazla destekli döşeme/merdiven yerleşimi; sıradan tek anchor snap'i değildir. |
| `log.step` / `log.gable` | Koru | Basamak ve üçgen dolgu gibi özel yerleşimler. |
| `log.strut` | Koru | Payandayı iki farklı destekle birlikte çözme. |
| `log.post_on_ground` | Koru | Zeminle ilişkili direk aralığı; bağlantı anchor'ı gibi ele alınmamalı. |
| `stone.on_anchor` / `stick.on_anchor` | Koru | Malzeme sağlayıcılarının ayrı uyumluluk kurallarıyla ortak çekirdeği kullanması. |
| Kararlı feature ID + geometri revizyonu | Koru | Dizi indeksleri kalıcı bağlantı kimliği olarak kullanılmamalı; değişmiş geometri eski preview'ı geçersiz kılmalı. |
| Tek çözüm → önizleme/commit | Koru ve sıkılaştır | Taşıma bileşeni sonradan bağımsız bir transform üretmemeli. |

## Tamamlanan tamamlayıcı bağlantılar

### 1. Yuvarlak istasyon → kütük ucu (`log.station_to_end`)

Önceki profiller uç→istasyon ve uç→uç yolunu kapsıyordu; tutulan kütüğün yan istasyonundan hedef kütüğün uç yüzüne giden yön eksikti. Bu profil yalnızca yuvarlak kütük istasyonlarından kaynak bağlantıya izin verir. `HeldYawOffset` ile gelen 90° dönüşler, uç düzlemindeki yönü seçmek için 0/90/180/270° ızgarasına oturur.

### 2. Yuvarlak istasyon → yuvarlak istasyon, 90° (`log.station_cross`)

Bu profil yalnızca iki `Round` kesitli istasyon arasında çalışır ve 90° çapraz temas üretir. İstasyon başına dört yönlü kopya oluşturulmaz: tek bir mantıksal istasyon, temas normali ve gövde teğeti çalışma anında çözülür. İki yüzeyin teğet olması hedeflenir; kütüklerin merkez çizgileri birbirinin içine geçirilmez.

Bu profil bir **temas/bağlantı** profilidir; fiziksel çentik geometrisi üretmez. Gerçek geçmeli köşe, saddle, V-notch veya cross-lap için açıkça eşleşen authored joinery anchor'ları kullanılmalı. Serbest açılı (15°–165°) çapraz birleşimler bu değişiklikte etkinleştirilmedi.

### 3. Authored anchor bağlantıları

Önceki `log.authored` profili `Any → Any` olduğu için uç ve istasyon davranışlarına da genel bir geri dönüş gibi davranabiliyordu. Artık yalnızca authored→authored için kullanılır. Uç→authored ve authored→uç ayrı profillerdir; gerçek uyumluluk yine anchor'ın tag/compatible-tag kurallarıyla reddedilebilir.

## Geometri politikası

- Kütüğün uçları gerçek mesh sınırlarından çıkarılır.
- Uzunluk istasyonları her tam 75 cm modülde bir üretilir; 600 cm kütükte 8 istasyon bulunur.
- Yuvarlak kesitte temas yönü sürekli çözülür (`RadialStepDeg = 0)); yontulmuş/düzleştirilmiş kesitte şu an dört yönlü yaklaşım korunur. Non-round yüzlerin mesh'e özgü tam face-frame/adreslenmiş anchor eşlemesi ayrı iyileştirme işidir.
- Snap istasyonları, 75 cm kesim sınırları ve eski 25 cm serbest kesim ayrı kavramlardır.
- Yan istasyonlar artık kaynak ve alıcı olabilir; hangi eşleşmenin mümkün olduğuna genel wildcard değil, profil filtresi ve sağlayıcı doğrulaması karar verir.

## Commit sözleşmesi

Bir aday sert kuralları geçmeden puanlanmamalı. Preview çözüldükten sonra source/target geometri revizyonları değişmişse commit reddedilmeli. İki destek isteyen beam/strut asla tek bağlantıya veya hinge fiziğine düşmemeli. Surface rest, structural connection ve receiver transfer birbirinden ayrı operation türleri olarak kalmalı.

## Gerekli kabul testleri

- End→end ve end→station mevcut testleri gerilememeli.
- Round station→end, doğru feature türlerini seçmeli; yüzey merkezi hedef uçtan bir yarıçap geride kalmalı.
- Round station→round station 90° profili iki yüzeyi teğet tutmalı ve kütük eksenlerini dik yapmalı.
- Non-round station'lar round-only çapraz profile girememeli.
- Dolu anchor, değişmiş geometry revision, bağlantı reddi ve rollback başarısızlığı durumlarında kısmi bağlantı kalmamalı.
- Gerçek mesh/pivot/collision ve UE 5.8 PIE kabulü ayrıca çalıştırılmalı; kaynak/test tanımı tek başına oyun içi kanıt değildir.
