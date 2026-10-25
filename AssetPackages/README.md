# Asset Packages

Bu klasör, projede kullanılacak asset paketlerinin ZIP arşivleri için gelen kutusudur.

## Kullanım
- Asset ZIP dosyalarını bu klasöre ekleyin.
- Her arşive içeriği anlatan kısa ve anlaşılır bir ad verin (ör. `Forest_Props_Pack.zip`).
- Varsa lisans, kaynak bağlantısı ve kullanım notlarını ZIP'in yanına bir `.txt` dosyasıyla ekleyin.
- Paketleri incelerken Unreal projesinin `Content/` dizinine doğrudan kopyalamak yerine önce dosya yapısı, lisans, bağımlılıklar ve boyut kontrolü yapılmalıdır.

## Büyük dosyalar
GitHub normal Git dosyaları için 100 MiB üzerindeki tekil dosya yüklemelerini engeller. Büyük arşivler için Git LFS veya harici dosya depolaması kullanın; LFS kotasını ve depolama/indirme bant genişliği limitlerini de kontrol edin.

> Not: Bu klasör paket arşivlerini toplamak içindir; assetlerin Unreal içinde otomatik olarak import edildiği anlamına gelmez.
