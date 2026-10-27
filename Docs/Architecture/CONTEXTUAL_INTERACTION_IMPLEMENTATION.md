# Bağlamsal Etkileşim — Uygulama Kaydı

**Hedef:** Oyuncunun her yerleştirme/kesme için bir profil seçmesi yerine, hedef geometrisi ve oyuncunun nişanından uygun işlemi otomatik çıkarmak.

**Tek çalışma zamanı çekirdeği:** `feature/global-snap-kernel` dalındaki `UPlacementKernelSubsystem`. Bu çalışma ikinci bir placement kernel oluşturmaz ve `main` dalını değiştirmez.

## Kabul edilen ilkeler

1. Önizleme ve commit aynı `FPlacementSolution` sonucunu kullanır.
2. Aday toplama, hedef geometri/özellik çözümlemesinden ayrıdır.
3. Yakın adaylar sınırlı bir collision overlap sorgusuyla bulunur; her yenilemede bütün dünyadaki aktörler taranmaz.
4. Çözücü, uyumluluk ve geometri kuralları geçmeden yalnızca yakınlık nedeniyle bir bağlantıyı kabul etmez.
5. Kesme varsayılanı nişangâh noktasından çözümlenir; özel yüzey işlemleri isteğe bağlı alternatif olarak kalır.
6. Unreal derlemesi ve PIE doğrulaması yapılmadan uygulama tamamlandı sayılmaz.

## Sıralı uygulama

| Adım | Durum | Kapsam |
|---|---|---|
| 1. Bağlamsal aday toplama | **Kaynak değişikliği yapıldı; Unreal doğrulaması bekliyor** | Nişan noktasının çevresindeki yerleştirilmiş kütükleri sınırlı overlap ile bulmak; doğrudan hedef yoksa en yakın gövdeyi tercih etmek; adayları `FPlacementIntent.Candidates` ile çekirdeğe geçirmek. |
| 2. Snap kararlılığı | **Kaynak değişikliği yapıldı; Unreal doğrulaması bekliyor** | Önizleme geçerli çözümdeyken nişan noktasının yakın bağlam içinde hareket etmesine izin vermek; küçük hareketlerde hedefi aniden silmemek. |
| 3. Nişanla kesme | **Varsayılan yol değiştirildi; Unreal doğrulaması bekliyor** | Varsayılan kesim `SplitLengthHalf` olur. `ResolveCutChoice` hedeflenen noktadan yarım/üç çeyrek/serbest uzunluk kesimini çözer; HUD mevcut `Share` değerini kesim halkası olarak gösterir. `C` ile özel işlem seçimi mevcut kalır. |
| 4. Yerleştirme niyetini sınıflandırma | **Kısmen uygulandı** | Yatay hedefin üst yüzeyine nişan alındığında istif çözümü yakın bağlam hedefi kullanır; yan yüzeyde istif zorlanmaz. Snap/stack/strut/zemin arasında tam tek karar akışı henüz kurulmadı. |
| 5. Eski karar yollarını birleştirme | **Bekliyor** | `UpdateStackPlacement`, `UpdateStrutPlacement`, uzak uç desteği ve commit sonrası tepkilerin birbiriyle çelişmesini önlemek. |
| 6. Kütük durum ve geometri sözleşmesi | **Bekliyor** | Mesh ölçüleri, pivot, kesit, feature revision ve kesim sonrası parça durumlarının doğrulanması. |
| 7. Taş/çubuk/halat sağlayıcıları | **Bekliyor** | Aynı çekirdeği kullanmak; her malzemenin kendine özgü yüzey ve bağlantı kurallarını korumak; halatın gerçek bağlantı uygulamasını tamamlamak. |
| 8. Kabul doğrulaması | **Bekliyor** | Kaynak incelemesi, otomasyon testleri, Unreal 5.8 derlemesi ve PIE senaryolarını ayrı sonuçlar olarak kaydetmek. |

## İlk kaynak değişiklikleri

- `LogCarryComponent`, yakın yerleştirme hedeflerini `OverlapMultiByChannel` ile sınırlandırılmış bir küre içinde arar.
- Varsayılan yatay istif, yalnızca hedef gövdenin yatay ve nişangâh noktasının üst tarafında olması durumunda devreye girer; hedefin yanına nişan alındığında istif sonucu zorla seçilmez.
- Geçerli doğrudan hedef tercih edilir; aksi halde nişangâh noktasına en yakın uygun kütük gövdesi, tek hedef gerektiren çözücüler için kullanılır.
- Çekirdeğe tüm bağlamsal adaylar verilir. Nihai feature eşleşmesini, profil filtresini ve çözümü hâlâ `UPlacementKernelSubsystem` belirler.
- Yerleştirme çözümü varken hedef aktör doğrudan nişangâh altında değilse, nişan noktası önizleme işaretinin etrafındaki ayarlanabilir bağlam yarıçapı içinde kaldığı sürece çözüm korunabilir.
- Stack çözümlemesi de doğrudan hedef aktöre kilitlenmez; bağlamsal aday toplama ile bulunan en uygun kütük kullanılır, ancak geometri kontrolü üst yüzey niyetini doğrulamak zorundadır.
- Varsayılan kesim eylemi `SplitLengthHalf` olarak ayarlandı. Mevcut HUD, nişan noktasından çözümlenen `Share` için `BuildCutRingSegments` kullanır; yani uzunluk kesiminin önizleme çizgisi hedeflenen konumu takip eder.

## Bilinçli olarak henüz yapılmayanlar

- `CyclePlacementMode` tamamen kaldırılmadı; serbest yerleştirme, dik direk ve özel geometriler için hangi alternatif girişlerin korunacağı bağlamsal sınıflandırma adımında belirlenecek.
- Eski stack/strut/support akışları bu ilk değişiklikte kaldırılmadı; önce yeni sınıflandırmanın yerini hangi davranışlarla doldurduğu doğrulanmalı.
- Kesit/çentik/oluk işlemleri otomatik olarak genel snap profiline dönüştürülmedi.
- Bu commitlerin hiçbiri Unreal derlemesi veya PIE testinin yerine geçmez.
