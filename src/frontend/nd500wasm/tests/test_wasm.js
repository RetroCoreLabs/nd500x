/* Node-based unit test for nd500wasm.js */
/* Assumes: nd500wasm.js exports malloc/free and JSON APIs via Module */

function makeAout(textBytes) {
  // ND-500 a.out header (32 bytes, 8x uint32 little-endian)
  const hdr = new Uint8Array(32);
  const view = new DataView(hdr.buffer);
  // a_magic = IMAGIC (0x0111 octal? Use 0x0109 per docs)
  view.setUint32(0, 0x0109, true);
  view.setUint32(4, textBytes.length, true);  // a_text
  view.setUint32(8, 0, true);                 // a_data
  view.setUint32(12, 0, true);                // a_bss
  view.setUint32(16, 0, true);                // a_syms
  view.setUint32(20, 0, true);                // a_entry (object → 0)
  view.setUint32(24, 0, true);                // a_trsize
  view.setUint32(28, 0, true);                // a_drsize
  const buf = new Uint8Array(hdr.length + textBytes.length);
  buf.set(hdr, 0);
  buf.set(textBytes, hdr.length);
  return buf;
}

function hex(u8) { return Array.from(u8, b => b.toString(16).padStart(2, '0')).join(' '); }

async function runTest() {
  return new Promise((resolve, reject) => {
    console.log('[TEST] starting');
    try {
      const create = require('../../../../build_wasm/bin/nd500wasm.js');
      create({}).then((Module) => {
        console.log('[TEST] Module initialized');
        // Initialize machine/CPU and anchor instruction table
        Module.ccall('nd500wasm_init', null, [], []);
        const icount = Module.ccall('nd500_dbg_instr_count_js', 'number', [], []);
        const mnem = Module.ccall('nd500_dbg_mnemonic_js', 'string', ['number'], [0x00B8]);
        console.log('[TEST] instr_count=', icount, ' mnemonic(0xB8)=', mnem);
        // Test payload (first 16 bytes from user's reference)
        const text = Uint8Array.from([
          0xB8,0xCF,0x1C,0x00,0x00,0x00,0x1A,0x08,
          0x44,0x0C,0x45,0x54,0x46,0xC2,0x05,0x00
        ]);
        const aout = makeAout(text);
        // Load via MEMFS path (same code path as browser UI)
        const path = '/test_math.o';
        try { Module.FS.unlink(path); } catch (e) {}
        Module.FS.writeFile(path, aout);
        const rc = Module.ccall('nd500_dbg_load_aout_path_js', 'number', ['string'], [path]);
        console.log('[TEST] load_aout_path_js rc=', rc);
        if (rc !== 0) throw new Error('load_aout_path_js failed rc=' + rc);

        // Verify memory dump
        const memJson = Module.ccall('nd500_dbg_mem_json', 'string', ['number','number'], [0, text.length]);
        const mem = JSON.parse(memJson);
        const got = mem.bytes.slice(0, text.length).join(' ').toLowerCase();
        const exp = hex(text);
        console.log('[TEST] mem[0..15]=', got);
        if (got !== exp) throw new Error('Memory mismatch\nexp: ' + exp + '\ngot: ' + got);

        // Verify disassembly returns some JSON (not empty)
        const disJson = Module.ccall('nd500_dbg_disasm_json', 'string', ['number','number'], [0, 128]);
        const dis = JSON.parse(disJson);
        console.log('[TEST] dis json=', disJson.substring(0, 200) + '...');
        console.log('[TEST] dis instructions count=', dis.instructions ? dis.instructions.length : 0);
        if (!dis.instructions || dis.instructions.length === 0) throw new Error('Empty disassembly instructions');

        console.log('WASM unit test OK');
        resolve();
      }).catch((e) => { console.error('[TEST] factory failed:', e); reject(e); });
    } catch (e) {
      console.error('[TEST] require failed:', e);
      reject(e);
    }
  });
}

runTest().then(() => process.exit(0)).catch(() => process.exit(1));


