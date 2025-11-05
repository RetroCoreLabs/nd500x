# Bit Editor - Browser Cache Troubleshooting

**Date**: October 15, 2025
**Issue**: Web UI still shows old numeric prompt instead of new bit editor

---

## Problem

After implementing the bit editor, the browser is still loading the old cached version of the JavaScript/CSS files.

## Solution

### Option 1: Hard Refresh (Recommended)

**Chrome/Edge/Firefox**:
```
Windows/Linux: Ctrl + Shift + R
macOS: Cmd + Shift + R
```

**Or**:
```
Windows/Linux: Ctrl + F5
macOS: Cmd + Option + R
```

### Option 2: Clear Browser Cache

**Chrome**:
1. Press `Ctrl + Shift + Delete` (or `Cmd + Shift + Delete` on Mac)
2. Select "Cached images and files"
3. Click "Clear data"
4. Reload page (`F5` or `Cmd + R`)

**Firefox**:
1. Press `Ctrl + Shift + Delete` (or `Cmd + Shift + Delete` on Mac)
2. Check "Cache"
3. Click "Clear Now"
4. Reload page (`F5` or `Cmd + R`)

**Edge**:
1. Press `Ctrl + Shift + Delete` (or `Cmd + Shift + Delete` on Mac)
2. Select "Cached images and files"
3. Click "Clear now"
4. Reload page (`F5` or `Cmd + R`)

### Option 3: Incognito/Private Mode

**Chrome**: `Ctrl + Shift + N` (or `Cmd + Shift + N` on Mac)
**Firefox**: `Ctrl + Shift + P` (or `Cmd + Shift + P` on Mac)
**Edge**: `Ctrl + Shift + N` (or `Cmd + Shift + N` on Mac)

Then navigate to `http://localhost:8080` (or wherever you're serving the files).

### Option 4: Disable Cache in DevTools

1. Open DevTools:
   - **Chrome/Edge**: `F12` or `Ctrl + Shift + I`
   - **Firefox**: `F12` or `Ctrl + Shift + I`

2. Go to **Network** tab

3. Check **"Disable cache"** checkbox

4. Keep DevTools open and reload the page

---

## Verification Steps

After clearing cache, verify the bit editor is working:

### 1. Check if Files Loaded
1. Open DevTools (`F12`)
2. Go to **Network** tab
3. Reload page
4. Look for `debugger.js?v=20251015` and `style.css?v=20251015`
5. Check that version query parameter is present

### 2. Test Bit-Mapped Register
1. Scroll to **Registers** panel on right side
2. Find **ST1** or **ST2** register
3. **Click on the register value**
4. **Expected**: Bit editor modal opens with 8-column grid
5. **If prompt dialog appears**: Cache not cleared properly, try hard refresh again

### 3. Test Numeric Register
1. Find **PC** register (Program Counter)
2. **Click on the register value**
3. **Expected**: Simple prompt dialog opens
4. Enter hex value like `0x08000000`
5. Register should update

---

## Technical Details

### Cache-Busting Version Numbers

The HTML now includes version query parameters:

```html
<link rel="stylesheet" href="style.css?v=20251015">
<script src="nd500wasm.js?v=20251015"></script>
<script src="debugger.js?v=20251015"></script>
```

This forces browsers to reload files when the version changes.

### Files Updated

- `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/index.html` - Added `?v=20251015` to all resource URLs
- `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/debugger.js` - Contains bit editor implementation
- `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/style.css` - Contains bit editor styles

---

## Still Not Working?

### Check Console for Errors

1. Open DevTools (`F12`)
2. Go to **Console** tab
3. Look for JavaScript errors (red text)
4. Common errors:
   - `Uncaught ReferenceError: getBitDefinition is not defined`
     → Old JavaScript file still cached
   - `Cannot read property 'classList' of null`
     → Old HTML file still cached

### Verify File Contents

Check that the files actually contain the new code:

```bash
# Check for bit editor modal in HTML
grep -c "bitEditorModal" /home/ronny/repos/nd500x/src/frontend/nd500wasm/web/index.html
# Should output: 1 or more

# Check for openBitEditor method in JS
grep -c "openBitEditor" /home/ronny/repos/nd500x/src/frontend/nd500wasm/web/debugger.js
# Should output: 2 or more

# Check for bit editor CSS
grep -c "bit-editor-modal" /home/ronny/repos/nd500x/src/frontend/nd500wasm/web/style.css
# Should output: 1 or more
```

### Restart HTTP Server

If using Python HTTP server:

```bash
# Stop server (Ctrl+C in terminal)
# Then restart:
cd /home/ronny/repos/nd500x/src/frontend/nd500wasm/web
python3 -m http.server 8080
```

Then navigate to `http://localhost:8080` and hard refresh (`Ctrl + Shift + R`).

---

## How to Test Without Browser

### Command-Line Verification

```bash
# Check that bit editor modal exists in HTML
curl http://localhost:8080/index.html | grep -c "bitEditorModal"
# Should output: 1 or more

# Check that debugger.js has bit editor code
curl http://localhost:8080/debugger.js | grep -c "openBitEditor"
# Should output: 2 or more

# Verify version parameter is present
curl -I http://localhost:8080/debugger.js?v=20251015
# Should return: HTTP/1.0 200 OK
```

---

## Expected Behavior After Fix

### Bit-Mapped Registers (ST1, ST2, OTE1, OTE2, CTE1, CTE2, MTE1, MTE2, TEMM1, TEMM2, FLAGS)

**When clicked**:
1. Modal appears with title "Edit Register: [NAME]"
2. Shows current value in hex (e.g., `0x00000000`)
3. Displays 8-column grid with 32 bits (4 rows × 8 columns)
4. Each bit shows:
   - Bit number (31 to 0)
   - Toggle (1 or 0)
   - Label (XSE, IIC, PE, PF, etc.)
5. Clicking a bit toggles it (blue = active)
6. Preview field updates in real-time
7. "Apply" button writes new value
8. "Cancel" button closes modal without saving

### Numeric Registers (PC, I1-I4, A1-A4, E1-E4, L, B, R, TOS, LL, HL, THA, PSTP, DITBASE, CED, CAD, PS)

**When clicked**:
1. Simple prompt dialog appears
2. Shows current value as default (e.g., `0x00000000`)
3. User enters hex (0x...) or decimal value
4. Press OK to apply or Cancel to abort
5. Register updates immediately

---

## Screenshots (What You Should See)

### Bit Editor Modal (ST1/ST2)

```
┌─────────────────────────────────────────────────┐
│ Edit Register: ST1                              │
├─────────────────────────────────────────────────┤
│ Current Value: 0x00001000                       │
│                                                  │
│ ┌───┬───┬───┬───┬───┬───┬───┬───┐              │
│ │31 │30 │29 │28 │27 │26 │25 │24 │              │
│ │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │              │
│ │PY │PA │PR │PT │PV │PG │PW │PL │              │
│ ├───┼───┼───┼───┼───┼───┼───┼───┤              │
│ │23 │22 │21 │20 │19 │18 │17 │16 │              │
│ │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │              │
│ │PC │PN │PX │PK │PM │PZ │PU │PO │              │
│ ├───┼───┼───┼───┼───┼───┼───┼───┤              │
│ │15 │14 │13 │12 │11 │10 │ 9 │ 8 │              │
│ │ 0 │ 0 │ 0 │█1█│ 0 │ 0 │ 0 │ 0 │  ← Bit 12   │
│ │PS │PD │PI │PF │PE │   │   │   │     active   │
│ ├───┼───┼───┼───┼───┼───┼───┼───┤              │
│ │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │              │
│ │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │              │
│ │   │   │   │   │   │   │   │   │              │
│ └───┴───┴───┴───┴───┴───┴───┴───┘              │
│                                                  │
│ New Value: 0x00001000                           │
│                                                  │
│ [Cancel]                    [Apply]             │
└─────────────────────────────────────────────────┘

Note: Bit 12 (PF - Divide by Zero) is active (blue)
```

### Simple Prompt (PC Register)

```
┌────────────────────────────────────┐
│ Enter new value for PC             │
│ (hex or decimal):                  │
│                                     │
│ ┌────────────────────────────────┐ │
│ │ 0x00000000                     │ │
│ └────────────────────────────────┘ │
│                                     │
│      [Cancel]         [OK]          │
└────────────────────────────────────┘
```

---

## Future: Automated Testing

To prevent cache issues in the future, consider:

1. **Build script** that auto-increments version on each build
2. **Service Worker** for proper cache invalidation
3. **Hash-based filenames** (e.g., `debugger.abc123.js`)

---

## Contact

If issues persist after trying all troubleshooting steps, check:
- `/home/ronny/repos/nd500x/docs/BIT_EDITOR_IMPLEMENTATION_COMPLETE.md` - Full implementation guide
- `/home/ronny/repos/nd500x/docs/ND500_REGISTER_REFERENCE.md` - Register reference

**Status**: ✅ Files updated with cache-busting version numbers
**Date**: October 15, 2025
