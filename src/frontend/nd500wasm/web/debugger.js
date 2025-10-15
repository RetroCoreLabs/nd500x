class ND500Debugger {
    constructor() {
        this.module = null;
        this.currentPC = 0;
        this.breakpoints = [];
        this.isRunning = false;
        this.runInterval = null;
    }

    async init() {
        console.log('ND500Debugger.init() called');
        // Wait for WASM module to load
        this.module = await Module;
        console.log('Module loaded:', this.module);

        // Get and display build info
        try {
            const buildInfo = this.module.ccall('nd500_dbg_build_info_js', 'string', [], []);
            console.log('WASM Build Info:', buildInfo);
            this.updateStatus('ND500X Web Debugger Ready - ' + buildInfo);
        } catch (e) {
            console.warn('Could not get build info:', e);
            this.updateStatus('ND500X Web Debugger Ready');
        }

        this.setupEventHandlers();
        console.log('Event handlers set up');
        this.setupDragDrop();
        console.log('Drag and drop set up');
        this.updateUI();
        console.log('UI updated');
        console.log('Debugger ready');
        // Auto-load demo kernel
        await this.loadDemoKernel();
    }

    setupEventHandlers() {
        // Load button opens modal
        document.getElementById('loadBtn').onclick = () => {
            console.log('Load button clicked');
            this.openLoadModal();
        };

        // Symbols button opens symbols modal
        document.getElementById('symbolsBtn').onclick = () => {
            console.log('Symbols button clicked');
            this.openSymbolsModal();
        };

        // Controls
        document.getElementById('stepBtn').onclick = () => this.step();
        document.getElementById('runBtn').onclick = () => this.run();
        document.getElementById('stopBtn').onclick = () => this.stop();
        document.getElementById('resetBtn').onclick = () => this.reset();

        // Memory
        document.getElementById('memViewBtn').onclick = () => this.viewMemory();

        // Breakpoints
        document.getElementById('bpAddBtn').onclick = () => this.addBreakpoint();
        document.getElementById('bpAddr').onkeypress = (e) => {
            if (e.key === 'Enter') this.addBreakpoint();
        };

        // Memory address input
        document.getElementById('memAddr').onkeypress = (e) => {
            if (e.key === 'Enter') this.viewMemory();
        };
        document.getElementById('memLen').onkeypress = (e) => {
            if (e.key === 'Enter') this.viewMemory();
        };
    }

    openLoadModal() {
        const modal = document.getElementById('loadModal');
        const typeRadios = document.getElementsByName('loadType');
        const aoutFields = document.getElementById('aoutFields');
        const splitFields = document.getElementById('splitFields');
        const cancelBtn = document.getElementById('loadCancelBtn');
        const confirmBtn = document.getElementById('loadConfirmBtn');
        const modeSelect = document.getElementById('modeSelect');
        const startPc = document.getElementById('startPc');

        const updateVisibility = () => {
            const type = Array.from(typeRadios).find(r => r.checked)?.value || 'aout';
            if (type === 'aout') {
                aoutFields.classList.remove('hidden');
                splitFields.classList.add('hidden');
            } else {
                aoutFields.classList.add('hidden');
                splitFields.classList.remove('hidden');
            }
        };
        Array.from(typeRadios).forEach(r => r.onchange = updateVisibility);
        updateVisibility();

        // Create error display element if it doesn't exist
        let errorDiv = modal.querySelector('.modal-error');
        if (!errorDiv) {
            errorDiv = document.createElement('div');
            errorDiv.className = 'modal-error hidden';
            modal.querySelector('.modal-content').insertBefore(errorDiv, modal.querySelector('.actions'));
        }

        cancelBtn.onclick = () => {
            modal.classList.add('hidden');
            errorDiv.classList.add('hidden');
            errorDiv.textContent = '';
        };

        confirmBtn.onclick = async () => {
            const type = Array.from(typeRadios).find(r => r.checked)?.value || 'aout';
            errorDiv.classList.add('hidden');
            errorDiv.textContent = '';

            try {
                if (type === 'aout') {
                    const file = document.getElementById('aoutFile').files[0];
                    if (!file) {
                        errorDiv.textContent = 'Please select an a.out file';
                        errorDiv.classList.remove('hidden');
                        return;
                    }
                    await this.loadAoutViaMemfs(file, startPc.value);
                } else {
                    const pseg = document.getElementById('psegFile').files[0] || null;
                    const dseg = document.getElementById('dsegFile').files[0] || null;
                    if (!pseg && !dseg) {
                        errorDiv.textContent = 'Please select at least PSEG or DSEG file';
                        errorDiv.classList.remove('hidden');
                        return;
                    }
                    const mode = modeSelect.value;
                    const psegAddrInput = document.getElementById('psegAddr').value.trim();
                    const dsegAddrInput = document.getElementById('dsegAddr').value.trim();

                    // Use custom addresses if provided, otherwise use mode defaults
                    let psegBase = (mode === 'kernel') ? (0x08000000 >>> 0) : (0xD0000000 >>> 0);
                    let dsegBase = (mode === 'kernel') ? (0x00000000 >>> 0) : (0xF0000000 >>> 0);

                    if (psegAddrInput) {
                        psegBase = this.parseHexOrDec(psegAddrInput) >>> 0;
                    }
                    if (dsegAddrInput) {
                        dsegBase = this.parseHexOrDec(dsegAddrInput) >>> 0;
                    }

                    await this.loadSplitViaMemfs(pseg, psegBase, dseg, dsegBase, startPc.value);
                }
                modal.classList.add('hidden');
                errorDiv.classList.add('hidden');
                errorDiv.textContent = '';
            } catch (err) {
                errorDiv.textContent = err.message;
                errorDiv.classList.remove('hidden');
                this.updateStatus('Load failed - see modal for details');
            }
        };

        modal.classList.remove('hidden');
    }

    openSymbolsModal() {
        if (!this.module) return;

        const modal = document.getElementById('symbolsModal');
        const cancelBtn = document.getElementById('symbolsCancelBtn');
        const symbolsList = document.getElementById('symbols-list');
        const searchInput = document.getElementById('symbolsSearchInput');
        const typeRadios = document.querySelectorAll('input[name="symbolType"]');

        try {
            // Get symbols from WASM
            const json = this.module.ccall('nd500_dbg_symbols_json', 'string', [], []);
            const symbols = JSON.parse(json);

            // Function to render symbols table (filtered or full)
            const renderSymbols = (filteredSymbols) => {
                if (filteredSymbols.length === 0) {
                    symbolsList.innerHTML = '<div class="symbol-empty">No symbols found</div>';
                } else {
                    // Create table of symbols
                    let html = '<table class="symbols-table">';
                    html += '<thead><tr><th>Name</th><th>Address</th><th>Type</th></tr></thead>';
                    html += '<tbody>';

                    filteredSymbols.forEach(sym => {
                        const addr = typeof sym.addr === 'number' ? sym.addr : parseInt(sym.addr, 10);
                        const addrHex = '0x' + addr.toString(16).padStart(8, '0').toUpperCase();
                        html += `<tr class="symbol-row" data-addr="${addr}">
                            <td class="symbol-name">${this.escapeHtml(sym.name)}</td>
                            <td class="symbol-addr">${addrHex}</td>
                            <td class="symbol-type">${this.escapeHtml(sym.type)}</td>
                        </tr>`;
                    });

                    html += '</tbody></table>';
                    symbolsList.innerHTML = html;

                    // Add click handlers to navigate to symbol addresses
                    document.querySelectorAll('.symbol-row').forEach(row => {
                        row.addEventListener('click', (e) => {
                            const addr = parseInt(e.currentTarget.dataset.addr);
                            // Set PC to the symbol address
                            this.module.ccall('nd500_dbg_set_pc_js', 'number', ['number'], [addr]);
                            this.updateStatus(`Navigated to ${e.currentTarget.querySelector('.symbol-name').textContent} at 0x${addr.toString(16).padStart(8,'0')}`);
                            // Close modal
                            modal.classList.add('hidden');
                            // Clear search input and reset filter
                            searchInput.value = '';
                            document.querySelector('input[name="symbolType"][value="all"]').checked = true;
                            // Update UI to reflect new PC
                            this.updateUI();
                        });
                    });
                }
            };

            // Combined filter function
            const filterSymbols = () => {
                const searchQuery = searchInput.value.toLowerCase().trim();
                const selectedType = Array.from(typeRadios).find(r => r.checked)?.value || 'all';

                let filtered = symbols;

                // Filter by type
                if (selectedType !== 'all') {
                    filtered = filtered.filter(sym => sym.type === selectedType);
                }

                // Filter by search text
                if (searchQuery) {
                    filtered = filtered.filter(sym =>
                        sym.name.toLowerCase().includes(searchQuery) ||
                        sym.type.toLowerCase().includes(searchQuery) ||
                        sym.addr.toString(16).toLowerCase().includes(searchQuery)
                    );
                }

                renderSymbols(filtered);
            };

            // Clear previous state and set up event handlers
            searchInput.value = '';
            document.querySelector('input[name="symbolType"][value="all"]').checked = true;

            // Attach event handlers
            searchInput.oninput = filterSymbols;
            typeRadios.forEach(radio => radio.onchange = filterSymbols);

            // Initial render with all symbols
            filterSymbols();

            // Focus search input for convenience
            setTimeout(() => searchInput.focus(), 100);

        } catch (error) {
            console.error('Error loading symbols:', error);
            symbolsList.innerHTML = '<div class="symbol-empty">Error loading symbols</div>';
        }

        // Cancel button closes modal
        cancelBtn.onclick = () => {
            modal.classList.add('hidden');
            searchInput.value = '';
            document.querySelector('input[name="symbolType"][value="all"]').checked = true;
        };

        modal.classList.remove('hidden');
    }

    async loadAoutViaMemfs(file, startPcValue) {
        console.log('=== loadAoutViaMemfs START ===');
        console.log('File:', file.name, 'Size:', file.size);

        // Clear memory and symbols before loading
        console.log('Clearing memory...');
        this.module.ccall('nd500_dbg_reset_memory_js', null, [], []);
        console.log('Clearing symbols...');
        this.module.ccall('nd500_dbg_clear_symbols_js', null, [], []);

        this.updateStatus('Loading a.out: ' + file.name + '...');
        const buffer = await file.arrayBuffer();
        const uint8 = new Uint8Array(buffer);
        const fname = '/upload_' + Date.now() + '.out';
        console.log('MEMFS path:', fname);

        if (this.module.FS && this.module.FS.writeFile) {
            console.log('Writing to MEMFS...');
            try { this.module.FS.unlink(fname); } catch (_) {}
            this.module.FS.writeFile(fname, uint8);
            console.log('File written to MEMFS, size:', uint8.length);

            console.log('Calling nd500_dbg_load_aout_path_js...');
            const rc = this.module.ccall('nd500_dbg_load_aout_path_js', 'number', ['string'], [fname]);
            console.log('Load result:', rc);
            if (rc !== 0) throw new Error('a.out load rc=' + rc);

            if (startPcValue && startPcValue.trim()) {
                const pc = this.parseHexOrDec(startPcValue.trim());
                console.log('Setting custom PC to:', '0x' + pc.toString(16));
                this.module.ccall('nd500_dbg_set_pc_js', 'number', ['number'], [pc >>> 0]);
            }

            this.updateStatus('a.out loaded: ' + file.name);
            console.log('Refreshing UI...');
            // Refresh UI to show new code and reset registers
            this.updateUI();
            // Set default memory view to start of loaded program
            document.getElementById('memAddr').value = '0';
            document.getElementById('memLen').value = '256';
            this.viewMemory();
            console.log('=== loadAoutViaMemfs COMPLETE ===');
        } else {
            throw new Error('MEMFS unavailable');
        }
    }

    parseHexOrDec(s) {
        const t = s.toLowerCase();
        if (t.startsWith('0x')) return parseInt(t, 16);
        return parseInt(t, 10);
    }

    async loadSplitViaMemfs(psegFile, psegBase, dsegFile, dsegBase, startPcValue) {
        console.log('=== loadSplitViaMemfs START ===');
        console.log('PSEG file:', psegFile ? psegFile.name : 'none', 'size:', psegFile ? psegFile.size : 0);
        console.log('PSEG base:', '0x' + psegBase.toString(16));
        console.log('DSEG file:', dsegFile ? dsegFile.name : 'none', 'size:', dsegFile ? dsegFile.size : 0);
        console.log('DSEG base:', '0x' + dsegBase.toString(16));
        console.log('Start PC:', startPcValue || '(default)');

        // Clear memory and symbols before loading
        console.log('Clearing memory...');
        this.module.ccall('nd500_dbg_reset_memory_js', null, [], []);
        console.log('Clearing symbols...');
        this.module.ccall('nd500_dbg_clear_symbols_js', null, [], []);

        const writeIf = async (file, path) => {
            if (!file) return null;
            console.log('Writing', file.name, 'to MEMFS path:', path);
            const buf = new Uint8Array(await file.arrayBuffer());
            try { this.module.FS.unlink(path); } catch (_) {}
            this.module.FS.writeFile(path, buf);
            console.log('Wrote', buf.length, 'bytes to', path);
            return path;
        };

        const psegPath = psegFile ? ('/upload_' + Date.now() + '.pseg') : null;
        const dsegPath = dsegFile ? ('/upload_' + (Date.now()+1) + '.dseg') : null;

        await writeIf(psegFile, psegPath);
        await writeIf(dsegFile, dsegPath);

        const setPc = !!(startPcValue && startPcValue.trim());
        const pc = setPc ? (this.parseHexOrDec(startPcValue.trim()) >>> 0) : (psegBase >>> 0);

        console.log('Calling nd500_dbg_load_segments_path_js with:');
        console.log('  psegPath:', psegPath, 'psegBase:', '0x' + (psegBase>>>0).toString(16));
        console.log('  dsegPath:', dsegPath, 'dsegBase:', '0x' + (dsegBase>>>0).toString(16));
        console.log('  setPc:', setPc, 'pc:', '0x' + pc.toString(16));

        const rc = this.module.ccall('nd500_dbg_load_segments_path_js', 'number',
            ['string','number','string','number','number','number'],
            [psegPath, psegBase>>>0, dsegPath, dsegBase>>>0, setPc?1:0, pc]);

        console.log('Load segments result:', rc);

        if (rc !== 0) {
            // Get detailed error message
            const attemptedAddr = psegFile ? psegBase : dsegBase;
            const errorMsg = this.module.ccall('nd500_dbg_load_strerror_js', 'string',
                ['number', 'number'], [rc, attemptedAddr >>> 0]);
            console.error('Load failed:', errorMsg);
            throw new Error(psegFile ? `PSEG load failed:\n${errorMsg}` : `DSEG load failed:\n${errorMsg}`);
        }

        if (!setPc) {
            console.log('Setting PC to:', '0x' + pc.toString(16));
            this.module.ccall('nd500_dbg_set_pc_js', 'number', ['number'], [pc]);
        }

        const segNames = [];
        if (psegFile) segNames.push(`PSEG@0x${psegBase.toString(16).toUpperCase()}`);
        if (dsegFile) segNames.push(`DSEG@0x${dsegBase.toString(16).toUpperCase()}`);
        this.updateStatus(`Loaded: ${segNames.join(', ')}`);

        console.log('Refreshing UI...');
        // Refresh UI to show new code and reset registers
        this.updateUI();
        // Set memory view to PSEG start if loaded, otherwise DSEG start
        const viewAddr = psegFile ? psegBase : dsegBase;
        document.getElementById('memAddr').value = viewAddr.toString(16).padStart(8, '0');
        document.getElementById('memLen').value = '256';
        this.viewMemory();
        console.log('=== loadSplitViaMemfs COMPLETE ===');
    }

    async loadDemoKernel() {
        try {
            this.updateStatus('Loading demo kernel...');
            const response = await fetch('kernel');
            if (!response.ok) throw new Error('Demo kernel not found');
            const buffer = await response.arrayBuffer();
            const uint8 = new Uint8Array(buffer);
            const fname = '/demo_kernel.out';
            if (this.module.FS && this.module.FS.writeFile) {
                try { this.module.FS.unlink(fname); } catch (_) {}
                this.module.FS.writeFile(fname, uint8);
                const rc = this.module.ccall('nd500_dbg_load_aout_path_js', 'number', ['string'], [fname]);
                if (rc !== 0) throw new Error('Demo kernel load failed rc=' + rc);
                this.updateStatus('Demo kernel loaded - NDIX-C Simulated Kernel v1.0 for ND-500');
                this.updateUI();
                // Set default memory view
                document.getElementById('memAddr').value = '0';
                document.getElementById('memLen').value = '256';
                this.viewMemory();
                return true;
            } else {
                throw new Error('MEMFS unavailable');
            }
        } catch (error) {
            console.warn('Could not load demo kernel:', error);
            this.updateStatus('ND500X Web Debugger Ready (demo kernel unavailable)');
            return false;
        }
    }

    setupDragDrop() {
        const dropZone = document.getElementById('drop-zone');
        
        document.body.ondragover = (e) => {
            e.preventDefault();
            dropZone.classList.remove('hidden');
        };
        
        document.body.ondragleave = (e) => {
            // Only hide if leaving the body entirely
            if (!e.relatedTarget || e.relatedTarget === document.body) {
                dropZone.classList.add('hidden');
            }
        };
        
        document.body.ondrop = (e) => {
            e.preventDefault();
            dropZone.classList.add('hidden');
            if (e.dataTransfer.files.length > 0) {
                this.loadFile(e.dataTransfer.files[0]);
            }
        };
    }

    async loadFile(file) {
        console.log('loadFile called with:', file);
        if (!file.name.match(/\.(o|out)$/i)) {
            this.updateStatus('Error: Please select a .o or .out file');
            return;
        }

        try {
            // Clear memory before loading
            this.module.ccall('nd500_dbg_reset_memory_js', null, [], []);
            this.updateStatus('Loading file: ' + file.name + '...');
            // Use MEMFS path loading for correctness across environments
            const buffer = await file.arrayBuffer();
            const uint8 = new Uint8Array(buffer);
            const fname = '/tmp_upload.o';
            // Ensure FS is available
            if (this.module.FS && this.module.FS.writeFile) {
                try { this.module.FS.unlink(fname); } catch (_) {}
                this.module.FS.writeFile(fname, uint8);
                console.log('Calling nd500_dbg_load_aout_path_js...');
                const result = this.module.ccall('nd500_dbg_load_aout_path_js', 'number', ['string'], [fname]);
                console.log('Load result (path):', result);
                if (result !== 0) throw new Error('Load failed rc=' + result);
            } else {
                // Fallback to direct pointer if FS is unavailable
                const ptr = this.module._malloc(uint8.length);
                this.module.HEAPU8.set(uint8, ptr);
                const result = this.module.ccall('nd500_dbg_load_aout_js', 'number', ['number','number'], [ptr, uint8.length]);
                this.module._free(ptr);
                if (result !== 0) throw new Error('Load failed rc=' + result);
            }
            
            {
                this.updateStatus('File loaded successfully: ' + file.name);
                // Refresh UI pieces explicitly to ensure PC/disasm are in sync
                this.updateRegisters();
                // Give the runtime a tick, then render disassembly
                setTimeout(() => this.updateDisassembly(), 0);
                // Set default memory view to start of loaded program
                document.getElementById('memAddr').value = '0';
                document.getElementById('memLen').value = '256';
                this.viewMemory();
            }
        } catch (error) {
            console.error('Error in loadFile:', error);
            this.updateStatus('Error loading file: ' + error.message);
        }
    }

    step() {
        if (!this.module) return;
        
        this.module.ccall('nd500_dbg_step_js', null, ['number'], [1]);
        this.updateUI();
        this.updateStatus('Stepped one instruction');
    }

    run() {
        if (!this.module || this.isRunning) return;
        
        this.isRunning = true;
        this.updateStatus('Running... (click Stop to halt)');
        
        // Update button states
        document.getElementById('runBtn').disabled = true;
        document.getElementById('stopBtn').disabled = false;
        document.getElementById('stepBtn').disabled = true;
        
        this.runInterval = setInterval(() => {
            this.module.ccall('nd500_dbg_step_js', null, ['number'], [100]);
            this.updateUI();
            
            // Check for breakpoints or manual stop
            const status = this.getStatus();
            if (!this.isRunning || status.breakpoint_hit) {
                this.stop();
            }
        }, 100);
    }

    stop() {
        if (!this.isRunning) return;
        
        this.isRunning = false;
        if (this.runInterval) {
            clearInterval(this.runInterval);
            this.runInterval = null;
        }
        
        this.module.ccall('nd500_dbg_stop_js', null, [], []);
        this.updateUI();
        this.updateStatus('Execution stopped');
        
        // Update button states
        document.getElementById('runBtn').disabled = false;
        document.getElementById('stopBtn').disabled = true;
        document.getElementById('stepBtn').disabled = false;
    }

    reset() {
        if (!this.module) return;
        
        this.stop();
        // Reset CPU state (implementation depends on available WASM functions)
        this.updateUI();
        this.updateStatus('CPU reset');
    }

    updateUI() {
        this.updateRegisters();
        this.updateTraps();
        this.updateDisassembly();
        this.updateBreakpoints();
    }

    updateRegisters() {
        if (!this.module) return;
        
        try {
            const json = this.module.ccall('nd500_dbg_regs_json', 'string', [], []);
            const regs = JSON.parse(json);
            this.currentPC = regs.PC;
            
            const html = `
                <div class="reg-item" data-reg="PC" data-value="${regs.PC}">PC: 0x${regs.PC.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="FLAGS" data-value="${regs.FLAGS}">FLAGS: 0x${regs.FLAGS.toString(16).padStart(8,'0')}</div>
                ${regs.I.map((v,i) => `<div class="reg-item" data-reg="I${i+1}" data-value="${v}">I${i+1}: 0x${v.toString(16).padStart(8,'0')}</div>`).join('')}
                ${regs.A.map((v,i) => `<div class="reg-item" data-reg="A${i+1}" data-value="${v}">A${i+1}: 0x${v.toString(16).padStart(8,'0')}</div>`).join('')}
                ${regs.E.map((v,i) => `<div class="reg-item" data-reg="E${i+1}" data-value="${v}">E${i+1}: 0x${v.toString(16).padStart(8,'0')}</div>`).join('')}
                <div class="reg-item" data-reg="L" data-value="${regs.L}">L: 0x${regs.L.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="B" data-value="${regs.B}">B: 0x${regs.B.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="R" data-value="${regs.R}">R: 0x${regs.R.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="TOS" data-value="${regs.TOS}">TOS: 0x${regs.TOS.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="LL" data-value="${regs.LL}">LL: 0x${regs.LL.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="HL" data-value="${regs.HL}">HL: 0x${regs.HL.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="THA" data-value="${regs.THA}">THA: 0x${regs.THA.toString(16).padStart(8,'0')}</div>
            `;
            document.getElementById('regs-content').innerHTML = html;
            
            // Add click handlers for register editing
            document.querySelectorAll('.reg-item').forEach(item => {
                item.addEventListener('click', (e) => {
                    this.editRegister(e.target);
                });
            });
        } catch (error) {
            console.error('Error updating registers:', error);
        }
    }

    updateDisassembly() {
        if (!this.module) return;

        try {
            const addr = Math.max(0, this.currentPC - 32);
            const json = this.module.ccall('nd500_dbg_disasm_json', 'string',
                ['number', 'number'], [addr, 128]);
            const data = JSON.parse(json);
            console.log('Disassembly JSON:', data);

            // Parse JSON instructions and render with PC highlighting in three lanes
            const lines = (data.instructions || []).map(inst => {
                const lineAddr = parseInt(inst.address, 16) || 0;
                const bytesStr = inst.bytes || '';
                const mnemonic = inst.mnemonic || '';
                const operands = inst.operands || '';
                const symbol = inst.symbol || '';
                const targetSymbol = inst.target_symbol || '';
                const relocSymbol = inst.reloc_symbol || '';
                const isUnresolved = inst.is_unresolved || false;
                const textStr = operands ? `${mnemonic} ${operands}` : mnemonic;

                const isCurrent = lineAddr === this.currentPC;
                const hasBP = this.breakpoints.some(bp => bp.addr === lineAddr);

                let classes = 'disasm-line';
                if (isCurrent) classes += ' current-pc';
                if (hasBP) classes += ' breakpoint';

                // Show symbol label on its own line if present
                let symbolLineHtml = '';
                if (symbol) {
                    symbolLineHtml = `<div class="disasm-line disasm-symbol"><span class="dis-symbol-label">${this.escapeHtml(symbol)}:</span></div>`;
                }

                const gutterHtml = `<span class="dis-gutter" title="Toggle breakpoint" onclick="nd500Debugger.toggleBreakpoint(${lineAddr}); event.stopPropagation();"></span>`;
                const addrHtml = `<span class="dis-addr">${inst.address}</span>`;
                const bytesHtml = `<span class="dis-bytes">${this.escapeHtml(bytesStr)}</span>`;

                // Use separate mnemonic and operands from JSON
                const isBranch = (/^(go|if|call)\b/i).test(mnemonic) || targetSymbol || relocSymbol;
                const headHtml = `<span class="dis-mnemonic${isBranch ? ' branch' : ''}">${this.escapeHtml(mnemonic)}</span>`;
                const opsHtml = operands ? `<span class="dis-operands">${this.escapeHtml(operands)}</span>` : '';

                // Add symbol comment for call/branch targets
                let commentHtml = '';
                if (relocSymbol) {
                    if (isUnresolved) {
                        commentHtml = `<span class="dis-comment"> ; ${this.escapeHtml(relocSymbol)} <span class="unresolved">(UNRESOLVED)</span></span>`;
                    } else {
                        commentHtml = `<span class="dis-comment"> ; -&gt; &lt;${this.escapeHtml(relocSymbol)}&gt;</span>`;
                    }
                } else if (targetSymbol) {
                    commentHtml = `<span class="dis-comment"> ; -&gt; &lt;${this.escapeHtml(targetSymbol)}&gt;</span>`;
                }

                const textHtml = `<span class="dis-text">${headHtml}${operands ? ' ' : ''}${opsHtml}${commentHtml}</span>`;

                const instructionHtml = `<div class="${classes}" data-addr="${lineAddr}">${gutterHtml}${addrHtml}${bytesHtml}${textHtml}</div>`;

                return symbolLineHtml + instructionHtml;
            }).join('');
            console.log('Rendered disasm lines:', (data.instructions || []).length);
            document.getElementById('disasm-content').innerHTML = lines;
        } catch (error) {
            console.error('Error updating disassembly:', error);
            document.getElementById('disasm-content').innerHTML = '<div class="disasm-line">Error loading disassembly</div>';
        }
    }

    viewMemory() {
        if (!this.module) return;

        try {
            const addr = parseInt(document.getElementById('memAddr').value, 16) || 0;
            const len = parseInt(document.getElementById('memLen').value) || 256;

            const json = this.module.ccall('nd500_dbg_mem_json', 'string',
                ['number', 'number'], [addr, len]);
            const data = JSON.parse(json);

            // Format as hex dump with ASCII - always show both columns
            let html = '';
            for (let i = 0; i < data.bytes.length; i += 16) {
                const lineAddr = data.addr + i;
                const hexBytes = data.bytes.slice(i, i+16);

                // Build hex string with proper spacing
                const hex = hexBytes.join(' ');

                // Build ASCII string
                let ascii = '';
                if (data.ascii) {
                    const asciiBytes = data.ascii.slice(i, i+16);
                    ascii = asciiBytes.join('');
                } else {
                    // Fallback if ASCII not available in JSON
                    ascii = hexBytes.map(b => {
                        const n = parseInt(b, 16);
                        return (n >= 32 && n <= 126) ? String.fromCharCode(n) : '.';
                    }).join('');
                }

                html += `<div class="mem-line">
                    <span class="mem-addr" data-addr="${lineAddr}">${lineAddr.toString(16).padStart(8,'0')}</span>
                    <span class="mem-hex">${hex}</span>
                    <span class="mem-ascii">${this.escapeHtml(ascii)}</span>
                </div>`;
            }
            document.getElementById('memory-content').innerHTML = html;
        } catch (error) {
            console.error('Error viewing memory:', error);
            document.getElementById('memory-content').innerHTML = '<div class="mem-line">Error loading memory</div>';
        }
    }

    toggleBreakpoint(addr) {
        if (!this.module) return;
        
        const existing = this.breakpoints.find(bp => bp.addr === addr);
        if (existing) {
            this.module.ccall('nd500_dbg_bp_del_js', 'number', ['number'], [existing.id]);
            this.updateStatus(`Breakpoint removed at 0x${addr.toString(16)}`);
        } else {
            const result = this.module.ccall('nd500_dbg_bp_add_js', 'number', ['number'], [addr]);
            if (result >= 0) {
                this.updateStatus(`Breakpoint added at 0x${addr.toString(16)}`);
            } else {
                this.updateStatus(`Failed to add breakpoint at 0x${addr.toString(16)}`);
            }
        }
        this.updateBreakpoints();
        this.updateDisassembly();
    }

    addBreakpoint() {
        if (!this.module) return;
        
        const addrStr = document.getElementById('bpAddr').value.trim();
        if (!addrStr) return;
        
        const addr = parseInt(addrStr, 16);
        if (isNaN(addr)) {
            this.updateStatus('Invalid address format. Use hex (e.g., 0x1000)');
            return;
        }
        
        const result = this.module.ccall('nd500_dbg_bp_add_js', 'number', ['number'], [addr]);
        if (result >= 0) {
            this.updateStatus(`Breakpoint added at 0x${addr.toString(16)}`);
            document.getElementById('bpAddr').value = '';
        } else {
            this.updateStatus(`Failed to add breakpoint at 0x${addr.toString(16)}`);
        }
        this.updateBreakpoints();
        this.updateDisassembly();
    }

    updateBreakpoints() {
        if (!this.module) return;
        
        try {
            const json = this.module.ccall('nd500_dbg_bp_list_json', 'string', [], []);
            this.breakpoints = JSON.parse(json);
            
            if (this.breakpoints.length === 0) {
                document.getElementById('bp-list').innerHTML = '<div style="color: #6c757d; font-style: italic;">No breakpoints set</div>';
                return;
            }
            
            const html = this.breakpoints.map(bp => `
                <div class="bp-item">
                    <input type="checkbox" ${bp.enabled ? 'checked' : ''} 
                        onchange="nd500Debugger.toggleBPEnabled(${bp.id}, this.checked)">
                    <span>0x${bp.addr.toString(16).padStart(8,'0')}</span>
                    <button onclick="nd500Debugger.deleteBP(${bp.id})">Del</button>
                </div>
            `).join('');
            document.getElementById('bp-list').innerHTML = html;
        } catch (error) {
            console.error('Error updating breakpoints:', error);
        }
    }

    toggleBPEnabled(id, enabled) {
        if (!this.module) return;
        
        if (enabled) {
            this.module.ccall('nd500_dbg_bp_enable_js', 'number', ['number'], [id]);
        } else {
            this.module.ccall('nd500_dbg_bp_disable_js', 'number', ['number'], [id]);
        }
        this.updateStatus(`Breakpoint ${id} ${enabled ? 'enabled' : 'disabled'}`);
    }

    deleteBP(id) {
        if (!this.module) return;
        
        this.module.ccall('nd500_dbg_bp_del_js', 'number', ['number'], [id]);
        this.updateStatus(`Breakpoint ${id} deleted`);
        this.updateBreakpoints();
        this.updateDisassembly();
    }

    getStatus() {
        if (!this.module) return { running: false, pc: 0, breakpoint_hit: false };
        
        try {
            const json = this.module.ccall('nd500_dbg_status_json', 'string', [], []);
            return JSON.parse(json);
        } catch (error) {
            console.error('Error getting status:', error);
            return { running: false, pc: 0, breakpoint_hit: false };
        }
    }

    updateStatus(msg) {
        document.getElementById('status-bar').textContent = msg;
    }

    updateTraps() {
        if (!this.module) return;
        
        try {
            const json = this.module.ccall('nd500_dbg_traps_json', 'string', [], []);
            const data = JSON.parse(json);
            const traps = data.traps || [];
            
            const content = document.getElementById('trap-content');
            if (traps.length === 0) {
                content.innerHTML = '<div class="trap-entry">No traps occurred</div>';
            } else {
                const html = traps.map(trap => 
                    `<div class="trap-entry">
                        <span class="trap-type">TRAP:</span>
                        <span class="trap-desc">${this.escapeHtml(trap.description)}</span>
                    </div>`
                ).join('');
                content.innerHTML = html;
            }
        } catch (error) {
            console.error('Error updating traps:', error);
            document.getElementById('trap-content').innerHTML = '<div class="trap-entry">Error loading trap information</div>';
        }
    }

    clearTraps() {
        if (!this.module) return;
        
        try {
            this.module.ccall('nd500_dbg_clear_traps_js', null, [], []);
            this.updateTraps();
            this.updateStatus('Traps cleared');
        } catch (error) {
            console.error('Error clearing traps:', error);
            this.updateStatus('Error clearing traps');
        }
    }

    editRegister(element) {
        const regName = element.dataset.reg;
        const currentValue = parseInt(element.dataset.value);
        
        const newValue = prompt(`Enter new value for ${regName} (hex or decimal):`, `0x${currentValue.toString(16).padStart(8,'0')}`);
        if (newValue === null) return; // User cancelled
        
        let value;
        if (newValue.startsWith('0x') || newValue.startsWith('0X')) {
            value = parseInt(newValue, 16);
        } else {
            value = parseInt(newValue, 10);
        }
        
        if (isNaN(value)) {
            alert('Invalid value. Please enter a valid hex (0x...) or decimal number.');
            return;
        }
        
        try {
            this.module.ccall('nd500_dbg_set_reg_js', null, ['string', 'number'], [regName, value]);
            this.updateStatus(`${regName} set to 0x${value.toString(16).padStart(8,'0')}`);
            
            // If PC was changed, update disassembly
            if (regName === 'PC') {
                this.updateDisassembly();
            }
            
            // Update all registers to reflect the change
            this.updateRegisters();
        } catch (error) {
            console.error('Error setting register:', error);
            this.updateStatus('Error setting register');
        }
    }

    setMemoryAddress(address) {
        const addrInput = document.getElementById('memAddr');
        if (addrInput) {
            addrInput.value = address.toString(16).padStart(8, '0');
            this.viewMemory();
        }
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}

// Initialize when WASM loads (support factory or legacy)
let nd500Debugger;

async function initWasmAndUI(mod) {
    console.log('WASM module initialized');
    try {
        console.log('Calling nd500wasm_init...');
        mod.ccall('nd500wasm_init', null, [], []);
        console.log('Creating ND500Debugger...');
        nd500Debugger = new ND500Debugger();
        window.nd500Debugger = nd500Debugger;
        console.log('Initializing debugger...');
        await nd500Debugger.init();
        console.log('Debugger initialized successfully');
        
        // Set up event handlers
        document.getElementById('clearTrapsBtn').addEventListener('click', () => {
            nd500Debugger.clearTraps();
        });
    } catch (error) {
        console.error('Failed to initialize debugger:', error);
        document.getElementById('status-bar').textContent = 'Failed to initialize debugger: ' + error.message;
    }
}

(function bootstrap() {
    if (typeof ND500Wasm === 'function') {
        console.log('Using modularized factory ND500Wasm');
        ND500Wasm({}).then((mod) => { window.Module = mod; initWasmAndUI(mod); })
        .catch((e) => {
            console.error('Failed to create WASM module:', e);
            document.getElementById('status-bar').textContent = 'Failed to create WASM module: ' + e.message;
        });
    } else {
        console.log('Using legacy global Module');
        if (typeof Module === 'undefined') window.Module = {};
        Module.onRuntimeInitialized = () => initWasmAndUI(Module);
    }
})();
