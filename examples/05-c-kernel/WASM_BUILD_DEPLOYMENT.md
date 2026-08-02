# WASM Build and Deployment Configuration

## Summary

The WASM build system now **automatically copies kernel.zip** from the source examples directory to the WASM build output, ensuring the web debugger always has the latest kernel with all 6 required files.

## Files Updated

### 1. Root Makefile

**File:** `Makefile`

**Target:** `wasm-serve`

```makefile
wasm-serve: wasm
	@echo "Copying kernel.zip to WASM build directory..."
	@cp examples/05-c-kernel/kernel.zip $(WASM_DIR)/bin/kernel.zip
	@echo "Starting web server on http://localhost:8000"
	@echo "Open http://localhost:8000 in your browser to use the ND500X Web Debugger"
	@cd $(WASM_DIR)/bin && python3 -m http.server 8000
```

**What it does:**
- Always copies fresh kernel.zip before starting web server
- Ensures no stale cached versions
- Works every time `make wasm-serve` is run

### 2. CMakeLists.txt

**File:** `src/frontend/nd500wasm/CMakeLists.txt`

**Changed from:**
```cmake
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/src/frontend/nd500wasm/web/demo/kernel.zip
    ${CMAKE_BINARY_DIR}/bin/kernel.zip
```

**Changed to:**
```cmake
COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/examples/05-c-kernel/kernel.zip
    ${CMAKE_BINARY_DIR}/bin/kernel.zip
```

**Key changes:**
1. ✓ Source changed from `src/frontend/nd500wasm/web/demo/` to `examples/05-c-kernel/`
2. ✓ Changed from `copy_if_different` to `copy` (always copy, no caching)

**Result:** Every WASM build copies the latest kernel.zip from the canonical source location.

### 3. JavaScript Cache Busting

**File:** `src/frontend/nd500wasm/web/debugger.js`

**Line 1935:**

```javascript
const zipResponse = await fetch('kernel.zip?v=' + Date.now());
```

**What it does:**
- Adds timestamp query parameter to prevent browser caching
- Forces browser to fetch fresh kernel.zip every time
- Works even if user doesn't clear browser cache

## Build Workflow

### Clean Build (Recommended)

```bash
cd .
make wasm
```

**Steps executed:**
1. Creates `build_wasm/` directory
2. Runs `emcmake cmake -DBUILD_WASM=ON ..`
3. Runs `make` in build_wasm
4. POST_BUILD copies files to `build_wasm/bin/`:
   - index.html
   - style.css
   - debugger.js (with cache-busting)
   - kernel (binary)
   - **kernel.zip** (from examples/05-c-kernel/)
   - test_*.html files

### Start Web Server

```bash
make wasm-serve
```

**Steps executed:**
1. Runs `make wasm` (if not already built)
2. **Copies kernel.zip** from `examples/05-c-kernel/` to `build_wasm/bin/`
3. Starts Python web server on port 8000
4. Serves files from `build_wasm/bin/`

### User Access

1. Open browser to `http://localhost:8000`
2. JavaScript fetches `kernel.zip?v=<timestamp>`
3. Browser bypasses cache (timestamp always unique)
4. ZIP extracted, all 6 files loaded:
   - locore.c, locore.s
   - kernel.c, kernel.s
   - kernel (executable)
   - kernel.map (symbol map)

## Verification

### Check WASM Build Output

```bash
ls -lh build_wasm/bin/kernel*
unzip -l build_wasm/bin/kernel.zip
```

**Expected output:**
```
Archive:  build_wasm/bin/kernel.zip
  Length      Date    Time    Name
---------  ---------- -----   ----
     2702  2025-11-16 00:35   locore.c
      499  2025-11-16 00:41   locore.s
     8553  2025-11-16 00:35   kernel.c
    10825  2025-11-16 00:41   kernel.s
     1624  2025-11-16 00:41   kernel
    16228  2025-11-16 00:41   kernel.map
---------                     -------
    40431                     6 files
```

### Check Source Location

```bash
md5sum examples/05-c-kernel/kernel.zip
md5sum build_wasm/bin/kernel.zip
```

**Expected:** Both checksums match: `052dc6146f636ee59531896542b100cf`

### Browser Console Verification

After opening http://localhost:8000, check browser console:

```
Fetching kernel.zip...
kernel.zip found, loading with source files...
[handleZipUpload] Starting ZIP extraction: kernel.zip
[handleZipUpload] ZIP file contents: (6) ['locore.c', 'locore.s', 'kernel.c', 'kernel.s', 'kernel', 'kernel.map']
[handleZipUpload] Detected as SOURCE file: locore.c
[handleZipUpload] Detected as SOURCE file: locore.s
[handleZipUpload] Detected as SOURCE file: kernel.c
[handleZipUpload] Detected as SOURCE file: kernel.s
[handleZipUpload] Detected as EXECUTABLE file: kernel
[handleZipUpload] Detected as MAP file: kernel.map
[handleZipUpload] Processing complete. Counts: {source: 4, map: 1, executable: 1, skipped: 0}
```

✓ Should see **6 files** (not 4)

## Canonical Source Location

**kernel.zip is built and stored at:**
```
examples/05-c-kernel/kernel.zip
```

**Built by:**
```bash
cd examples/05-c-kernel
make all
```

**Deployed to:**
```
build_wasm/bin/kernel.zip           (by CMake POST_BUILD)
src/frontend/nd500wasm/web/demo/kernel.zip  (manual copy if needed)
```

## Updating Kernel

### Step 1: Rebuild Kernel

```bash
cd examples/05-c-kernel
make clean
make all
```

This creates fresh `kernel.zip` with all 6 files.

### Step 2: Rebuild WASM (Automatic Copy)

```bash
cd .
make wasm
```

CMake POST_BUILD automatically copies updated kernel.zip to `build_wasm/bin/`.

### Step 3: Serve (Force Fresh Copy)

```bash
make wasm-serve
```

Makefile target copies kernel.zip again before starting server (double insurance).

### Step 4: Browser Refresh

Open http://localhost:8000 and **hard refresh** (Ctrl+Shift+R).

Cache-busting timestamp ensures fresh ZIP is fetched.

## Troubleshooting

### Problem: Browser Still Shows 4 Files

**Symptom:**
```
[handleZipUpload] ZIP file contents: (4) ['kernel.c', 'kernel.s', 'kernel', 'kernel.map']
```

**Causes:**
1. Browser cache not cleared
2. Web server serving old file
3. kernel.zip not rebuilt

**Solutions:**

**Option 1: Hard Refresh**
```
Ctrl+Shift+R (or Cmd+Shift+R on Mac)
```

**Option 2: Clear Browser Cache**
```
Open DevTools → Application → Clear Storage → Clear Site Data
```

**Option 3: Rebuild Everything**
```bash
cd .
rm -rf build_wasm
make wasm
make wasm-serve
```

**Option 4: Verify Source File**
```bash
unzip -l examples/05-c-kernel/kernel.zip
# Should show 6 files
```

### Problem: kernel.zip Has Only 4 Files

**Symptom:**
```bash
unzip -l examples/05-c-kernel/kernel.zip
# Shows only: kernel.c, kernel.s, kernel, kernel.map
```

**Cause:** Old kernel.zip from before locore.c was added.

**Solution:**
```bash
cd examples/05-c-kernel
make clean
make all
unzip -l kernel.zip  # Verify 6 files
```

### Problem: POST_BUILD Doesn't Copy

**Symptom:** `build_wasm/bin/kernel.zip` doesn't exist or is outdated.

**Cause:** CMake cache issue.

**Solution:**
```bash
rm -rf build_wasm
make wasm
```

Clean rebuild forces CMake to re-run POST_BUILD commands.

## File Locations Reference

| Location | Purpose | Updated By |
|----------|---------|------------|
| `examples/05-c-kernel/kernel.zip` | **Canonical source** | `make all` in examples/05-c-kernel/ |
| `build_wasm/bin/kernel.zip` | WASM runtime | CMake POST_BUILD + `make wasm-serve` |
| `src/frontend/nd500wasm/web/demo/kernel.zip` | Legacy backup | Manual copy (optional) |

## Build System Flow

```
examples/05-c-kernel/kernel.c + locore.c
           ↓ (make all)
examples/05-c-kernel/kernel.zip  ← CANONICAL SOURCE
           ↓ (CMake POST_BUILD)
build_wasm/bin/kernel.zip
           ↓ (make wasm-serve copies again)
build_wasm/bin/kernel.zip  ← SERVED TO BROWSER
           ↓ (HTTP GET with ?v=timestamp)
Browser downloads kernel.zip
           ↓ (JSZip extracts)
6 files available in debugger
```

## Summary

**Problem Solved:** Browser was seeing old cached kernel.zip with only 4 files.

**Root Causes:**
1. CMakeLists.txt copied from wrong location
2. Makefile wasm-serve didn't force copy
3. JavaScript had no cache-busting

**Solutions Implemented:**
1. ✓ CMakeLists.txt now copies from `examples/05-c-kernel/`
2. ✓ CMakeLists.txt uses `copy` (not `copy_if_different`)
3. ✓ Makefile `wasm-serve` copies kernel.zip before serving
4. ✓ JavaScript adds timestamp to fetch URL

**Result:**
- Every build gets fresh kernel.zip
- Every web server start gets fresh kernel.zip
- Every browser request gets fresh kernel.zip
- All 6 files (including locore.c, locore.s) now available

**Verification Command:**
```bash
make wasm-serve
# Then open http://localhost:8000
# Check browser console - should see 6 files
```

**Expected Console Output:**
```
[handleZipUpload] ZIP file contents: (6) ['locore.c', 'locore.s', 'kernel.c', 'kernel.s', 'kernel', 'kernel.map']
ZIP loaded: 1 executable(s), 4 source file(s), 1 map file(s)
```

✓ **All files including locore.c and locore.s are now available in WASM debugger!**
