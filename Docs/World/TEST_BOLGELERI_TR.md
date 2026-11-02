# Test bölgeleri: v4, v5, v7, v9, v12 (2026-11-02)

Ölçümler test sahnelerinden (`/Game/World/Test/Test_<id>`, haritanın tamamı 1 m/px, ~4×4 km) ve kimlik kartlarından.
İnşaata uygun = 12°'den düz; yürünür = 35°'den düz; uçurum = 44°'den dik (karakter çıkamaz).

| | v4 | v5 | v7 | v9 | v12 |
|---|---|---|---|---|---|
| Karakter | sarp sırtlar, dar vadiler | kayalık yarlı engebeli yayla | volkanik koniler | yüksek plato | geniş yumuşak tepeler |
| Yükseklik | 0–230 m | 0–300 m | 0–225 m | 0–296 m | 0–162 m |
| Ortanca eğim | **19,5°** (en dik) | 13,9° | 13,4° | 15,5° | **9,1°** (en yumuşak) |
| İnşaata uygun | **26 %** | 43 % | 44 % | 34 % | **70 %** |
| Yürünür | 91 % | 89 % | 96 % | 98 % | 100 % |
| Uçurum | 1,1 % | **5,0 %** | 1,0 % | 0,3 % | 0 % |
| Orman | 29 % | 36 % | 35 % | 34 % | 35 % |
| Tepe / çukur | 73 / 50 | 80 / 66 | **87** / 61 | 81 / **24** | 40 / 25 |
| Arazinin ağırlığı | alçakta (derin vadiler) | ortada | alçakta, tepeler tek tek | **yüksekte** | ortada |
| Kenarlar | hepsi alçak | orta, batı yüksek | alçak-orta | **yüksek**, doğu en yüksek | orta, düzgün |

## v4 — sarp dağlık
Keskin sırtlar ve ağaç dalı gibi dallanan dar vadiler. Arazinin çoğu vadi tabanında; sırtlar ince ve sık.
En dik ortalama, en az düz yer. Kenarları alçak, yani her yönde vadiyle bitiyor: ova veya vadi bölgesine kolay bağlanır.
Not: 1 m/px'te dağlar "sık sarp tepeler" gibi görünüyor; gerçek ölçeğinde (kart: ~14 km) büyük dağ sırası olur.
Oyunda: zorlu geçiş, taş/maden, mağara, gözetleme sırtları; üs için az yer.

## v5 — engebeli yayla (şu anki World)
Düzlüklerin arasında kayalık yarlar; en çok uçurum bunda (%5). Çok sayıda küçük tepe ve çukur.
Düzlükler geniş (%43 inşaata uygun) ama yarlarla bölünmüş. Batı kenarı daha yüksek.
Oyunda: çeşitli; yarlar doğal duvar ve taş kaynağı, çukurlar küçük gölet adayı, mağara için uygun yamaçlar.

## v7 — volkanik koniler
Tek tek yükselen koniler ve zirvelerden ışın gibi açılan sırtlar; en çok tepe (87). Arazinin çoğu alçakta, koniler
aralarda ada gibi. Yürümesi kolay (%96), düz yer bol (%44).
Oyunda: uzaktan görülen yön işaretleri; konilerin etekleri yerleşime uygun; mantar kolonileri/spor bacaları için doğal yerler.

## v9 — yüksek plato (v8 dosyası)
Arazinin çoğu yüksekte, üstü geniş ve hafif dalgalı; kenarlardan yarıntılarla iniyor. En az çukur (24): su tutmuyor,
akıp gidiyor. Kenarları yüksek, doğusu en yüksek: alçak bir bölgeye doğrudan bağlanamaz, arada yamaç/yar gerekir.
Oyunda: yüksek, açık, rüzgârlı ve soğuk alan; savunma ve gözetleme; su kıt.

## v12 — geniş yumuşak tepeler
En yumuşak arazi: ortanca 9°, %70 inşaata uygun, uçurum yok. Belirgin dere yolları, az çukur. Kenarları düzgün ve orta yükseklikte:
her bölgeyle birleşebilir.
Oyunda: ilk üs, tarım, orman; bölgeler arası geçiş/dolgu alanı.

## Birlikte
Yükseklik sırası (arazinin ağırlığına göre): v9 (yüksek) → v5, v12 (orta) → v4, v7 (alçak, ama sivri sırt/koniler).
Doğal dizilim önerisi: v9 plato kenarda/kuzeyde, v4 ile v7 onun eteklerinde, v5 ortada, v12 en alçak ve yaşanabilir tarafta.
Bu beşinde göl/ova/vadi tabanı yok (v10, v3, v6 bunlardı); gerekirse adım 3'te eksik coğrafya olarak eklenir.
