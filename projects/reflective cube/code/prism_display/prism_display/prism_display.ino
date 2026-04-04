/*
 * Prism Display - ESP32-S3
 * 
 * Hardware:
 *   - ESP-WROOM-32 Development Board
 *   - SPI TFT Display (ST7789 240x240) via TFT_eSPI
 *   - W25Q64 SPI Flash (8MB) for image/gif storage
 *   - KY-040 Rotary Encoder for navigation
 * 
 * Libraries required (install via Arduino Library Manager):
 *   - TFT_eSPI        (Bodmer)
 *   - JPEGDEC         (bitbank2)
 *   - AnimatedGIF     (bitbank2)
 *   - SPIMemory       (Marzogh)
 *   - ESPAsyncWebServer + AsyncTCP
 * 
 * TFT_eSPI User_Setup.h — must match these exactly:
 *   #define ST7789_DRIVER
 *   #define TFT_WIDTH  240
 *   #define TFT_HEIGHT 240
 *   #define TFT_MOSI   23
 *   #define TFT_SCLK   18
 *   #define TFT_CS     15
 *   #define TFT_DC     2
 *   #define TFT_RST    4
 *   #define SPI_FREQUENCY  40000000
 * 
 * SPI Bus Split:
 *   TFT  → VSPI  (GPIO 18/23, hardware default)
 *   Flash → HSPI (GPIO 14/12/13) — separate bus, no conflict
 * 
 * Pin Definitions:
 *   TFT VSPI:   MOSI=23, SCLK=18, CS=15, DC=2,  RST=4
 *   Flash HSPI: MOSI=13, SCLK=14, MISO=12, CS=5
 *   KY-040:     CLK=32,  DT=33,   SW=25
 */

// ===================== INCLUDES =====================
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <SPIMemory.h>
#include <AnimatedGIF.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// ===================== CONFIG =====================
// WiFi credentials — change these
#define WIFI_SSID     "It Hurts When IP"
#define WIFI_PASSWORD "Getsomemeds!"

// TFT uses VSPI (managed entirely by TFT_eSPI via User_Setup.h)
// TFT pins: MOSI=23, SCLK=18, CS=15, DC=2, RST=4

// Flash uses HSPI — completely separate SPI bus, no conflict with TFT
#define FLASH_CS_PIN   5
#define FLASH_MOSI     13
#define FLASH_MISO     12
#define FLASH_SCLK     14

// Encoder
#define ENCODER_CLK    32
#define ENCODER_DT     33
#define ENCODER_SW     25

// Display size
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 240

// Flash layout
// Sector size on W25Q64 is 4096 bytes — must erase full sector before writing
// Sector 0 (0x0000–0x0FFF): File table only — never overwritten by image data
// Sector 1+ (0x1000+):      Image/GIF data
#define FILE_TABLE_ADDR    0
#define FILE_TABLE_MAX     50          // 50 entries x 40 bytes = 2000 bytes, fits in sector 0
#define FILE_ENTRY_SIZE    40
#define FILE_DATA_START    0x1000      // Start of sector 1 — safely away from file table

// Encoder modes
#define MODE_BROWSE     0   // rotate = next/prev image
#define MODE_BRIGHTNESS 1   // rotate = brightness up/down

// ===================== GLOBALS =====================
TFT_eSPI tft = TFT_eSPI();

// Flash gets its own HSPI bus — isolated from TFT's VSPI
SPIClass flashSPI(HSPI);
SPIFlash flash(FLASH_CS_PIN, &flashSPI);

// Poll the W25Q64 status register directly until WIP (Write In Progress) bit clears
// This bypasses SPIMemory's internal timeout and actually waits for completion
void flashWaitReady() {
  flashSPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(FLASH_CS_PIN, LOW);
  flashSPI.transfer(0x05); // Read Status Register-1 command
  uint32_t start = millis();
  while (true) {
    uint8_t status = flashSPI.transfer(0x00);
    if (!(status & 0x01)) break; // WIP bit clear = done
    if (millis() - start > 5000) {
      Serial.println("  flashWaitReady timeout!");
      break;
    }
  }
  digitalWrite(FLASH_CS_PIN, HIGH);
  flashSPI.endTransaction();
}

AnimatedGIF gif;
WebServer server(80);
Preferences prefs;

// File table entry
struct FileEntry {
  uint32_t offset;
  uint32_t size;
  char     name[32];
};

FileEntry fileTable[FILE_TABLE_MAX];
int       fileCount    = 0;
int       currentIndex = 0;
int       brightness   = 128;   // 0–255 (PWM on backlight if wired)
int       currentMode  = MODE_BROWSE;

// Encoder state
volatile int  encoderPos     = 0;
volatile int  lastEncoderPos = 0;
volatile bool buttonPressed  = false;
int           lastCLK;

// GIF playback
bool     isPlayingGIF  = false;
uint8_t* gifBuffer     = nullptr;
uint32_t gifBufferSize = 0;

// Upload buffer — declared here so showStaticImage() can reuse it
// Allocated in setup() before WiFi starts
static const uint32_t uploadMaxSize = 100 * 1024;
static uint8_t* uploadBuf    = nullptr;
static uint32_t uploadSize   = 0;
static bool     uploadActive = false;
static char     uploadName[64];

// ===================== FLASH FILE TABLE =====================

void loadFileTable() {
  fileCount = 0;
  for (int i = 0; i < FILE_TABLE_MAX; i++) {
    uint32_t addr = FILE_TABLE_ADDR + i * FILE_ENTRY_SIZE;
    FileEntry entry;
    flash.readByteArray(addr, (uint8_t*)&entry, FILE_ENTRY_SIZE);
    if (entry.offset == 0xFFFFFFFF || entry.size == 0 || entry.size == 0xFFFFFFFF) break;
    fileTable[fileCount++] = entry;
  }
  Serial.printf("Loaded %d files from flash table\n", fileCount);
}

void saveFileTable() {
  flash.eraseSector(0x0000);
  flashWaitReady();
  for (int i = 0; i < fileCount; i++) {
    uint32_t addr = FILE_TABLE_ADDR + i * FILE_ENTRY_SIZE;
    flash.writeByteArray(addr, (uint8_t*)&fileTable[i], FILE_ENTRY_SIZE, true);
    flashWaitReady();
  }
  if (fileCount < FILE_TABLE_MAX) {
    FileEntry sentinel;
    memset(&sentinel, 0xFF, sizeof(sentinel));
    uint32_t addr = FILE_TABLE_ADDR + fileCount * FILE_ENTRY_SIZE;
    flash.writeByteArray(addr, (uint8_t*)&sentinel, FILE_ENTRY_SIZE, true);
    flashWaitReady();
  }
}

// Returns flash address where new file data can be written
uint32_t nextFreeAddr() {
  if (fileCount == 0) return FILE_DATA_START;
  FileEntry& last = fileTable[fileCount - 1];
  uint32_t next = last.offset + last.size;
  // Align to 256-byte page boundary
  if (next % 256 != 0) next += 256 - (next % 256);
  return next;
}

// Write a file to flash and register it in the table
// Returns true on success
bool writeFileToFlash(const char* name, uint8_t* data, uint32_t size) {
  if (fileCount >= FILE_TABLE_MAX) {
    Serial.println("File table full!");
    return false;
  }
  uint32_t addr = nextFreeAddr();
  uint32_t flashSize = flash.getCapacity();
  if (addr + size > flashSize) {
    Serial.println("Not enough flash space!");
    return false;
  }

  Serial.printf("Writing %s (%u bytes) at flash addr 0x%08X\n", name, size, addr);

  // Erase sectors covering this file with proper busy-wait
  uint32_t sectorSize  = 4096;
  uint32_t firstSector = (addr / sectorSize) * sectorSize;
  uint32_t lastSector  = ((addr + size - 1) / sectorSize) * sectorSize;
  for (uint32_t sAddr = firstSector; sAddr <= lastSector; sAddr += sectorSize) {
    Serial.printf("  Erasing sector at 0x%08X\n", sAddr);
    flash.eraseSector(sAddr);
    flashWaitReady(); // Poll status register until truly done
  }

  // Write in 256-byte pages
  uint32_t written = 0;
  while (written < size) {
    uint32_t chunk = min((uint32_t)256, size - written);
    if (!flash.writeByteArray(addr + written, data + written, chunk)) {
      Serial.printf("  Write failed at offset %u, error: %02x\n", written, flash.error());
      return false;
    }
    written += chunk;
  }

  // Verify first two bytes are correct
  uint8_t verify[2];
  flash.readByteArray(addr, verify, 2);
  Serial.printf("  Verify header: 0x%02X 0x%02X\n", verify[0], verify[1]);

  // Register in table
  FileEntry entry;
  entry.offset = addr;
  entry.size   = size;
  strncpy(entry.name, name, 31);
  entry.name[31] = '\0';
  fileTable[fileCount++] = entry;
  saveFileTable();

  Serial.printf("File written and registered: %s\n", name);
  return true;
}

void deleteAllFiles() {
  Serial.println("Deleting all files...");
  flash.eraseSector(0x0000);
  flashWaitReady();
  for (uint32_t addr = FILE_DATA_START; addr < FILE_DATA_START + (512 * 1024); addr += 4096) {
    flash.eraseSector(addr);
    flashWaitReady();
  }
  fileCount    = 0;
  currentIndex = 0;
  memset(fileTable, 0xFF, sizeof(fileTable));
  flash.eraseChip();
  Serial.println("All files deleted.");
}

// ===================== DISPLAY HELPERS =====================

bool isGIF(const char* name) {
  int len = strlen(name);
  return (len > 4 && strcasecmp(name + len - 4, ".gif") == 0);
}

// Read callback for AnimatedGIF from a RAM buffer
void* gifOpen(const char* fname, int32_t* pSize) {
  *pSize = gifBufferSize;
  return (void*)gifBuffer;
}
void gifClose(void* pHandle) {}
int32_t gifRead(GIFFILE* pFile, uint8_t* pBuf, int32_t iLen) {
  int32_t avail = pFile->iSize - pFile->iPos;
  if (iLen > avail) iLen = avail;
  memcpy(pBuf, gifBuffer + pFile->iPos, iLen);
  pFile->iPos += iLen;
  return iLen;
}
int32_t gifSeek(GIFFILE* pFile, int32_t iPosition) {
  pFile->iPos = iPosition;
  return iPosition;
}

// Draw a single GIF frame to TFT
void gifDraw(GIFDRAW* pDraw) {
  uint16_t* usPalette = pDraw->pPalette;
  int       y         = pDraw->iY + pDraw->y;
  uint8_t*  s         = pDraw->pPixels;
  uint16_t  usTemp[DISPLAY_WIDTH];

  for (int x = 0; x < pDraw->iWidth; x++) {
    usTemp[x] = usPalette[s[x]];
  }
  tft.pushImage(pDraw->iX, y, pDraw->iWidth, 1, usTemp);
}

void showStaticImage(int index) {
  if (index < 0 || index >= fileCount) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.print("No images!");
    return;
  }

  FileEntry& f = fileTable[index];
  Serial.printf("Displaying: %s (%u bytes)\n", f.name, f.size);

  // Reuse the upload buffer — it's always allocated and never needed at the same time as display
  if (!uploadBuf) {
    Serial.println("No image buffer available!");
    return;
  }
  if (f.size > uploadMaxSize) {
    Serial.printf("Image too large: %u > %u bytes\n", f.size, uploadMaxSize);
    return;
  }

  uint8_t* buf = uploadBuf;
  flash.readByteArray(f.offset, buf, f.size);

  if (isGIF(f.name)) {
    // For GIF we still need a separate persistent buffer since playback is ongoing
    uint8_t* gifBuf = (uint8_t*)malloc(f.size);
    if (!gifBuf) {
      Serial.println("malloc failed for GIF buffer");
      return;
    }
    memcpy(gifBuf, buf, f.size);
    if (gifBuffer) free(gifBuffer);
    gifBuffer     = gifBuf;
    gifBufferSize = f.size;
    isPlayingGIF  = true;
  } else {
    // JPEG: decode directly from upload buffer
    Serial.printf("Decoding JPEG: %u bytes, heap free: %u\n", f.size, ESP.getFreeHeap());
    // Verify JPEG header (should start with FF D8)
    Serial.printf("Header bytes: 0x%02X 0x%02X\n", buf[0], buf[1]);
    tft.fillScreen(TFT_BLACK);
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback([](int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) -> bool {
      Serial.printf("  draw block x=%d y=%d w=%d h=%d\n", x, y, w, h);
      tft.pushImage(x, y, w, h, bitmap);
      return true;
    });
    bool ok = TJpgDec.drawJpg(0, 0, buf, f.size);
    Serial.printf("drawJpg returned: %d\n", ok);
    isPlayingGIF = false;
  }
}

void showCurrentFile() {
  isPlayingGIF = false;
  if (gifBuffer) {
    free(gifBuffer);
    gifBuffer = nullptr;
  }
  showStaticImage(currentIndex);
}

void showInfoOverlay() {
  if (fileCount == 0) return;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.printf("%d/%d %s", currentIndex + 1, fileCount, fileTable[currentIndex].name);
}

void showBrightnessOverlay() {
  tft.fillRect(40, 110, 160, 20, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(44, 112);
  tft.printf("Brightness: %d%%", (brightness * 100) / 255);
  // Draw a simple bar
  tft.drawRect(40, 125, 160, 8, TFT_WHITE);
  tft.fillRect(42, 127, (156 * brightness) / 255, 4, TFT_YELLOW);
}

// ===================== ENCODER ISR =====================

void IRAM_ATTR encoderISR() {
  int clk = digitalRead(ENCODER_CLK);
  int dt  = digitalRead(ENCODER_DT);
  if (clk != lastCLK) {
    if (dt != clk) encoderPos++;
    else           encoderPos--;
    lastCLK = clk;
  }
}

void IRAM_ATTR buttonISR() {
  buttonPressed = true;
}

// ===================== WIFI & WEB SERVER =====================

void startWiFi() {
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);

  // Disable power saving — prevents TCP core locking assert with AsyncWebServer
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
    delay(500); // Let TCP stack fully settle before server.begin()
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(2);
    tft.setCursor(10, 80);
    tft.print("WiFi Connected");
    tft.setCursor(10, 110);
    tft.setTextSize(1);
    tft.printf("Upload at:\nhttp://%s", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi failed.");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.print("WiFi Failed");
  }
}

void setupWebServer() {

  // Serve upload page
  server.on("/", HTTP_GET, []() {
    String html = R"rawhtml(
<!DOCTYPE html><html>
<head><title>Prism Display</title>
<style>
  body{font-family:Arial,sans-serif;max-width:500px;margin:40px auto;padding:20px;background:#111;color:#eee;}
  h2{color:#7df;}
  input[type=file],input[type=submit]{width:100%;padding:10px;margin:8px 0;border-radius:6px;border:none;}
  input[type=submit]{background:#27ae60;color:#fff;cursor:pointer;font-size:16px;}
  input[type=submit]:hover{background:#2ecc71;}
  #filelist{margin-top:20px;}
  .fileitem{padding:6px;border-bottom:1px solid #333;}
  .status{color:#2ecc71;margin-top:10px;}
</style></head>
<body>
<h2>&#128302; Prism Display</h2>
<p>Upload JPEG or GIF images to display on your prism.</p>
<form id="uploadForm" enctype="multipart/form-data">
  <input type="file" id="fileInput" name="file" accept=".jpg,.jpeg,.gif" required>
  <input type="submit" value="Upload Image">
</form>
<div class="status" id="status"></div>
<div id="filelist"><h3>Stored Files</h3><div id="files">Loading...</div></div>
<button onclick="deleteAll()" style="background:#c0392b;color:#fff;padding:10px;border:none;border-radius:6px;cursor:pointer;width:100%;margin-top:10px;">Delete All Files</button>
<script>
document.getElementById('uploadForm').onsubmit = async(e) => {
  e.preventDefault();
  const fd = new FormData();
  fd.append('file', document.getElementById('fileInput').files[0]);
  document.getElementById('status').innerText = 'Uploading...';
  const r = await fetch('/upload', {method:'POST', body:fd});
  const t = await r.text();
  document.getElementById('status').innerText = t;
  loadFiles();
};
async function loadFiles() {
  const r = await fetch('/files');
  const j = await r.json();
  const el = document.getElementById('files');
  if (!j.files || j.files.length === 0) { el.innerHTML = '<i>No files stored</i>'; return; }
  el.innerHTML = j.files.map((f,i) =>
    `<div class="fileitem">${i+1}. ${f.name} (${(f.size/1024).toFixed(1)}KB)</div>`
  ).join('');
}
async function deleteAll() {
  if (!confirm('Delete all files?')) return;
  document.getElementById('status').innerText = 'Deleting...';
  const r = await fetch('/delete_all', {method:'POST'});
  const t = await r.text();
  document.getElementById('status').innerText = t;
  loadFiles();
}
loadFiles();
</script>
</body></html>
)rawhtml";
    server.send(200, "text/html", html);
  });

  // List files as JSON
  server.on("/files", HTTP_GET, []() {
    String json = "{\"files\":[";
    for (int i = 0; i < fileCount; i++) {
      if (i > 0) json += ",";
      json += "{\"name\":\"" + String(fileTable[i].name) + "\",\"size\":" + String(fileTable[i].size) + "}";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

  // Delete all files
  server.on("/delete_all", HTTP_POST, []() {
    Serial.println("Delete all requested via web...");
    deleteAllFiles();
    // Verify flash is actually blank at data start
    uint8_t verify[4];
    flash.readByteArray(FILE_DATA_START, verify, 4);
    Serial.printf("Post-delete verify at 0x1000: 0x%02X 0x%02X 0x%02X 0x%02X\n",
                  verify[0], verify[1], verify[2], verify[3]);
    bool blank = (verify[0] == 0xFF && verify[1] == 0xFF && verify[2] == 0xFF && verify[3] == 0xFF);
    Serial.printf("Flash blank check: %s\n", blank ? "PASS" : "FAIL — still has data!");
    String msg = blank
      ? "All files deleted. Flash verified blank."
      : "Delete ran but flash verify FAILED — try again.";
    server.send(200, "text/plain", msg);
  });

  // File upload — WebServer handles multipart via onFileUpload + handler pair
  server.on("/upload", HTTP_POST,
    // Request complete handler
    []() {
      if (uploadActive && uploadSize > 0) {
        bool ok = writeFileToFlash(uploadName, uploadBuf, uploadSize);
        uploadSize   = 0;
        uploadActive = false;
        if (ok) server.send(200, "text/plain", "Upload successful!");
        else    server.send(500, "text/plain", "Flash write failed");
      } else {
        server.send(400, "text/plain", "No data received or file too large");
      }
    },
    // Chunk handler — called for each piece of the uploaded file
    []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        uploadSize   = 0;
        uploadActive = true;
        strncpy(uploadName, upload.filename.c_str(), 63);
        uploadName[63] = '\0';
        Serial.printf("Upload start: %s (buf=%u bytes)\n", uploadName, uploadMaxSize);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadActive && (uploadSize + upload.currentSize) <= uploadMaxSize) {
          memcpy(uploadBuf + uploadSize, upload.buf, upload.currentSize);
          uploadSize += upload.currentSize;
        } else {
          Serial.printf("WARNING: file too large, max %u bytes\n", uploadMaxSize);
          uploadActive = false;
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        Serial.printf("Upload complete: %u bytes\n", uploadSize);
      }
    }
  );

  server.begin();
  Serial.println("Web server started.");
}

// ===================== SETUP =====================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Prism Display Boot ===");

  // ── 0. Grab upload buffer FIRST before WiFi/flash eat the heap ──
  uploadBuf = (uint8_t*)malloc(uploadMaxSize);
  if (!uploadBuf) {
    Serial.println("FATAL: could not allocate upload buffer!");
  } else {
    Serial.printf("Upload buffer: %u bytes allocated, heap free: %u\n",
                  uploadMaxSize, ESP.getFreeHeap());
  }

  // ── 1. Flash on HSPI — init before TFT touches anything ──
  SPI.begin(FLASH_SCLK, FLASH_MISO, FLASH_MOSI, FLASH_CS_PIN); // SCK, MISO, MOSI, CS
  delay(50);
  flashSPI.begin(FLASH_SCLK, FLASH_MISO, FLASH_MOSI, FLASH_CS_PIN);
  delay(10);
  if (!flash.begin()) {
    Serial.println("W25Q64 flash not found! Check wiring.");
  } else {
    Serial.printf("Flash OK — capacity: %u bytes\n", flash.getCapacity());
    loadFileTable();
  }

  // ── 2. TFT on VSPI — managed by TFT_eSPI, won't touch HSPI ──
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.print("Prism Display");
  tft.setTextSize(1);
  tft.setCursor(20, 130);
  tft.print("Booting...");

  // Rotary encoder init
  lastCLK = digitalRead(ENCODER_CLK);
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT,  INPUT_PULLUP);
  pinMode(ENCODER_SW,  INPUT_PULLUP);
  lastCLK = digitalRead(ENCODER_CLK);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), encoderISR,  CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_SW),  buttonISR,   FALLING);

  // Load saved preferences
  prefs.begin("prism", false);
  currentIndex = prefs.getInt("index", 0);
  brightness   = prefs.getInt("bright", 200);
  prefs.end();

  // WiFi + web server
  startWiFi();
  setupWebServer();

  // Show first image
  if (fileCount > 0) {
    if (currentIndex >= fileCount) currentIndex = 0;
    showCurrentFile();
    showInfoOverlay();
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(20, 100);
    tft.print("No images stored.");
    tft.setCursor(20, 116);
    tft.printf("Upload at http://%s", WiFi.localIP().toString().c_str());
  }

  Serial.println("Setup complete.");
}

// ===================== LOOP =====================

unsigned long lastOverlayTime   = 0;
bool          overlayVisible    = false;
unsigned long lastBrightnessShow = 0;
bool          brightnessVisible = false;
int           gifFrameDelay     = 0;
unsigned long lastGifFrame      = 0;

void loop() {
  // --- Rotary encoder: position changed ---
  if (encoderPos != lastEncoderPos) {
    int delta = encoderPos - lastEncoderPos;
    lastEncoderPos = encoderPos;

    if (currentMode == MODE_BROWSE) {
      // Navigate images
      if (fileCount > 0) {
        currentIndex = (currentIndex + delta + fileCount) % fileCount;
        showCurrentFile();
        showInfoOverlay();
        overlayVisible  = true;
        lastOverlayTime = millis();
        // Save position
        prefs.begin("prism", false);
        prefs.putInt("index", currentIndex);
        prefs.end();
      }
    } else if (currentMode == MODE_BRIGHTNESS) {
      brightness = constrain(brightness + delta * 8, 0, 255);
      // If you have a PWM-capable backlight pin, set it here:
      // analogWrite(BACKLIGHT_PIN, brightness);
      showBrightnessOverlay();
      brightnessVisible    = true;
      lastBrightnessShow   = millis();
      prefs.begin("prism", false);
      prefs.putInt("bright", brightness);
      prefs.end();
    }
  }

  // --- Button press: toggle mode ---
  if (buttonPressed) {
    buttonPressed = false;
    currentMode = (currentMode == MODE_BROWSE) ? MODE_BRIGHTNESS : MODE_BROWSE;
    Serial.printf("Mode switched to: %s\n", currentMode == MODE_BROWSE ? "BROWSE" : "BRIGHTNESS");
    // Show mode indicator briefly
    tft.fillRect(0, DISPLAY_HEIGHT - 16, DISPLAY_WIDTH, 16, TFT_BLACK);
    tft.setTextColor(currentMode == MODE_BROWSE ? TFT_CYAN : TFT_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(4, DISPLAY_HEIGHT - 12);
    tft.print(currentMode == MODE_BROWSE ? "MODE: Browse" : "MODE: Brightness");
    lastOverlayTime  = millis();
    overlayVisible   = true;
  }

  // --- GIF playback ---
  if (isPlayingGIF && gifBuffer) {
    if (millis() - lastGifFrame >= gifFrameDelay) {
      static bool gifOpen = false;
      if (!gifOpen) {
        gif.begin(BIG_ENDIAN_PIXELS);
        if (gif.open(gifBuffer, gifBufferSize, gifDraw)) {
          gifOpen = true;
          Serial.printf("GIF: %dx%d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
        }
      }
      if (gifOpen) {
        int result = gif.playFrame(true, &gifFrameDelay);
        lastGifFrame = millis();
        if (result == 0) {
          // GIF finished — loop it
          gif.reset();
        }
      }
    }
  }

  // --- Auto-hide info overlay after 2 seconds ---
  if (overlayVisible && millis() - lastOverlayTime > 2000) {
    overlayVisible = false;
    // Redraw that region from the image (just refresh full image for simplicity)
    if (!isPlayingGIF) showCurrentFile();
  }

  // --- Auto-hide brightness overlay after 1.5 seconds ---
  if (brightnessVisible && millis() - lastBrightnessShow > 1500) {
    brightnessVisible = false;
    if (!isPlayingGIF) showCurrentFile();
  }

  delay(5); // Small yield
  server.handleClient(); // Process any incoming web requests
}
