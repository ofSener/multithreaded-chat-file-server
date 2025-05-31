# Testing Guide - Multi-threaded Chat and File Server

## 📋 Overview

Bu kılavuz, multi-threaded chat ve file server projesinin test edilmesi için hazırlanmış otomatik test script'lerinin kullanımını açıklar.

## 🚀 Test Script'leri

### 1. **Quick Test Script** (`quick_test.sh`)
Temel fonksiyonaliteyi hızlıca test eder (1-2 dakika).

### 2. **Comprehensive Test Suite** (`test_suite.sh`)
Rapordaki tüm test senaryolarını detaylı olarak test eder (5-10 dakika).

## 🔧 Kurulum ve Hazırlık

### Gereksinimler
```bash
# Gerekli araçlar
- bash (4.0+)
- make
- gcc
- netstat
- timeout
- mkfifo
```

### Dosya İzinleri
```bash
chmod +x quick_test.sh
chmod +x test_suite.sh
```

## 🏃‍♂️ Hızlı Test (Quick Test)

### Kullanım
```bash
./quick_test.sh
```

### Test Edilen Özellikler
1. ✅ **Compilation Test**: Proje derleme kontrolü
2. ✅ **Server Startup**: Sunucu başlatma testi
3. ✅ **Client Connection**: Client bağlantı testi
4. ✅ **Logging Functionality**: Loglama sistemi testi
5. ✅ **File Transfer System**: Dosya transfer sistemi başlatma
6. ✅ **Graceful Shutdown**: Zarif kapatma testi

### Örnek Çıktı
```
=== QUICK TEST SUITE ===
[20:30:15] ℹ️  Testing compilation...
[20:30:16] ✅ Compilation test passed
[20:30:16] ℹ️  Testing server startup...
[20:30:18] ✅ Server startup test passed
[20:30:18] ℹ️  Testing client connection...
[20:30:21] ✅ Client connection test passed
[20:30:21] ℹ️  Testing logging functionality...
[20:30:21] ✅ Logging test passed
[20:30:21] ℹ️  Log entries: 4
[20:30:21] ℹ️  Testing file transfer system...
[20:30:21] ✅ File transfer system test passed
[20:30:21] ℹ️  Testing graceful shutdown...
[20:30:23] ✅ Graceful shutdown test passed

=== QUICK TEST COMPLETED ===
[20:30:23] ℹ️  For comprehensive testing, run: ./test_suite.sh
[20:30:23] ℹ️  Server log: server_log.txt
```

## 🔬 Kapsamlı Test (Comprehensive Test Suite)

### Kullanım
```bash
./test_suite.sh
```

### Test Edilen Senaryolar

#### **Test 1: Concurrent User Load Test**
- 15 eşzamanlı client bağlantısı
- Temel iletişim testi
- Server yük testi

#### **Test 2: Username Validation Test**
- Duplicate username reddi
- Geçersiz karakter kontrolü
- Username format doğrulaması

#### **Test 3: Room Management Test**
- Oda oluşturma
- Oda katılma/ayrılma
- Boş oda yok etme

#### **Test 4: Message Broadcasting Test**
- Oda içi mesaj yayını
- Mesaj teslimatı kontrolü
- Multi-client iletişim

#### **Test 5: Private Messaging (Whisper) Test**
- Özel mesaj gönderimi
- Hedef kullanıcı doğrulaması
- Mesaj gizliliği

#### **Test 6: File Transfer Test**
- Geçerli dosya transferi
- Upload queue sistemi
- Dosya kaydetme kontrolü

#### **Test 7: File Validation Test**
- Büyük dosya reddi (>3MB)
- Geçersiz dosya tipi reddi
- Var olmayan dosya kontrolü

#### **Test 8: Error Handling Test**
- Oda olmadan broadcast
- Var olmayan kullanıcıya whisper
- Oda olmadan leave

#### **Test 9: Client Exit Test**
- `/exit` komutu testi
- Process temizleme
- Disconnect loglama

#### **Test 10: Server Shutdown Test**
- SIGINT ile zarif kapatma
- Client bildirim sistemi
- Resource temizleme

### Test Çıktı Dosyaları

#### **Test Results Directory** (`test_results/`)
```
test_results/
├── test_report.txt           # Kapsamlı test raporu
├── server_output.txt         # Server çıktısı
├── client_alice_output.txt   # Client çıktıları
├── client_bob_output.txt
├── client_*_input.txt        # Client input pipe'ları
└── ...
```

#### **Test Files Directory** (`test_files/`)
```
test_files/
├── test.txt                  # Geçerli text dosyası
├── sample.pdf               # Geçerli PDF dosyası
├── large_file.txt           # Büyük dosya (>3MB)
└── malware.exe              # Geçersiz dosya tipi
```

### Örnek Kapsamlı Test Çıktısı
```
================================
MULTI-THREADED CHAT SERVER TEST SUITE
================================
[20:35:00] ℹ️  Starting comprehensive test suite...

================================
SETTING UP TEST ENVIRONMENT
================================
[20:35:00] ℹ️  Compiling project...
[20:35:02] ✅ Test environment setup completed
[20:35:02] ℹ️  Starting server on port 8080...
[20:35:04] ✅ Server started successfully (PID: 12345)

================================
TEST 1: CONCURRENT USER LOAD TEST
================================
[20:35:04] ℹ️  Starting 15 concurrent clients...
[20:35:04] ℹ️  Started client: alice
[20:35:05] ℹ️  Started client: bob
...
[20:35:10] ✅ Concurrent user test passed (15 users connected)
[20:35:10] ℹ️  Testing basic communication...
[20:35:12] ✅ Basic communication test passed

================================
TEST 2: USERNAME VALIDATION TEST
================================
[20:35:15] ℹ️  Testing duplicate username rejection...
[20:35:17] ✅ Duplicate username rejection test passed
[20:35:17] ℹ️  Testing invalid username rejection...
[20:35:19] ✅ Invalid username rejection test passed

... (diğer testler)

================================
GENERATING TEST REPORT
================================
[20:40:30] ✅ Test report generated: test_results/test_report.txt

================================
TEST SUITE COMPLETED
================================
[20:40:30] ✅ All tests completed. Check test_results for detailed results.
[20:40:30] ℹ️  Server log: server_log.txt
[20:40:30] ℹ️  Test report: test_results/test_report.txt
```

## 📊 Test Raporu Analizi

### Server Log Analizi
Test tamamlandıktan sonra `server_log.txt` dosyasında:
```
2025-05-31 20:35:02 - [INFO] Server listening on port 8080...
2025-05-31 20:35:02 - [FILE-INIT] File transfer system initialized with 5 workers
2025-05-31 20:35:05 - [CONNECT] New client connected from 127.0.0.1
2025-05-31 20:35:05 - [LOGIN] user 'alice' connected from 127.0.0.1
...
```

### Test Report (`test_results/test_report.txt`)
```
Multi-threaded Chat and File Server - Test Report
Generated on: Fri May 31 20:40:30 2025

=== TEST ENVIRONMENT ===
Server Host: 127.0.0.1
Server Port: 8080
Test Directory: test_results

=== SERVER LOG ANALYSIS ===
Total log entries: 156
Connections: 25
Logins: 23
Room operations: 12
Messages: 8
File transfers: 3
Errors: 2
```

## 🐛 Troubleshooting

### Yaygın Sorunlar ve Çözümler

#### **1. Port Already in Use**
```bash
# Çözüm: Çalışan server'ları temizle
pkill -f chatserver
pkill -f chatclient
```

#### **2. Permission Denied**
```bash
# Çözüm: Script izinlerini kontrol et
chmod +x quick_test.sh
chmod +x test_suite.sh
```

#### **3. Compilation Errors**
```bash
# Çözüm: Dependencies kontrol et
make clean
make all
```

#### **4. Test Timeout**
```bash
# Çözüm: System load kontrol et, test'i tekrar çalıştır
./quick_test.sh  # Önce basit test
```

#### **5. Named Pipe Errors**
```bash
# Çözüm: Eski pipe'ları temizle
rm -f test_results/client_*_input.txt
```

### Debug Modu
```bash
# Verbose output için
bash -x ./test_suite.sh

# Specific test için
bash -x ./quick_test.sh
```

## 📈 Test Metrikleri

### Başarı Kriterleri
- ✅ **Compilation**: Hatasız derleme
- ✅ **Concurrency**: 15+ eşzamanlı client
- ✅ **Messaging**: Broadcast ve whisper çalışıyor
- ✅ **File Transfer**: Queue sistemi aktif
- ✅ **Error Handling**: Tüm hata durumları yakalanıyor
- ✅ **Logging**: Kapsamlı log kaydı
- ✅ **Shutdown**: Zarif kapatma

### Performance Beklentileri
- **Connection Time**: < 2 saniye
- **Message Delivery**: < 1 saniye
- **File Transfer**: < 5 saniye (küçük dosyalar)
- **Shutdown Time**: < 3 saniye

## 🎯 Test Sonuçlarını Değerlendirme

### Tam Başarı (All Green ✅)
Tüm testler geçti - Proje production ready!

### Kısmi Başarı (Some Yellow ⚠️)
Bazı testler inconclusive - Manuel kontrol gerekli

### Başarısızlık (Red ❌)
Test başarısız - Kod düzeltmesi gerekli

## 📝 Rapor Hazırlama

Test sonuçlarını akademik raporda kullanmak için:

1. **Test Çıktılarını Kaydet**:
   ```bash
   ./test_suite.sh > test_output.txt 2>&1
   ```

2. **Log Dosyalarını Kopyala**:
   ```bash
   cp server_log.txt test_results/
   cp test_results/test_report.txt final_report/
   ```

3. **Screenshots Al**:
   - Test çıktıları
   - Server logları
   - Client etkileşimleri

Bu test script'leri, projenizin tüm gereksinimlerini karşıladığını kanıtlamak için kapsamlı bir test suite sağlar! 🚀 