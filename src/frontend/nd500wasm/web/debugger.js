class ND500Debugger {
    constructor() {
        this.module = null;
        this.currentPC = 0;
        this.breakpoints = [];
        this.isRunning = false;
        this.runInterval = null;
        this.VERSION = '20251015d'; // Update this with each change
    }

    async init() {
        console.log('='.repeat(60));
        console.log('ND500X Web Debugger');
        console.log('Version:', this.VERSION);
        console.log('='.repeat(60));
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

        // MMU button opens MMU modal
        document.getElementById('mmuBtn').onclick = () => {
            console.log('MMU button clicked');
            this.openMmuModal();
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

        // Register editing - event delegation on parent
        document.getElementById('regs-content').addEventListener('click', (e) => {
            const regItem = e.target.closest('.reg-item');
            if (regItem) {
                this.editRegister(regItem);
            }
        });
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

        const domainSelect = document.getElementById('domainSelect');

        // Check and display MMU status
        const mmuStatusBanner = document.getElementById('loadModalMmuStatus');
        const mmuStatusValue = document.getElementById('loadModalMmuValue');
        const mmuStatusHint = document.getElementById('loadModalMmuHint');
        const mmuToggleBtn = document.getElementById('loadModalMmuToggle');

        const updateMmuStatus = () => {
            if (this.module) {
                const mmuEnabled = this.module.ccall('nd500_dbg_mmu_is_enabled_js', 'number', [], []);
                const mode = modeSelect.value;

                // Get Mode and Domain rows
                const modeRow = modeSelect.closest('.row');
                const domainRow = domainSelect.closest('.row');
                const psegAddrInput = document.getElementById('psegAddr');
                const dsegAddrInput = document.getElementById('dsegAddr');

                if (mmuEnabled) {
                    // MMU Enabled: Show Mode and Domain, use virtual addresses
                    mmuStatusValue.textContent = '✓ Enabled';
                    mmuStatusValue.style.color = '#4CAF50';
                    mmuStatusBanner.style.backgroundColor = '#e8f5e9';
                    mmuToggleBtn.textContent = 'Disable MMU';
                    mmuToggleBtn.style.backgroundColor = '#f44336';

                    // Show Mode and Domain fields
                    if (modeRow) modeRow.style.display = '';
                    if (domainRow) domainRow.style.display = '';

                    // Update hint and placeholders based on mode
                    if (mode === 'kernel') {
                        mmuStatusHint.textContent = 'Virtual addresses: Code 0x08000000-0x0FFFFFFF, Data 0x00000000-0x07FFFFFF';
                        if (psegAddrInput) psegAddrInput.placeholder = 'Default: 0x08000000 (kernel code)';
                        if (dsegAddrInput) dsegAddrInput.placeholder = 'Default: 0x00000000 (kernel data)';
                    } else {
                        mmuStatusHint.textContent = 'Virtual addresses: Code 0xD0000000-0xD7FFFFFF, Data 0xF0000000-0xF7FFFFFF';
                        if (psegAddrInput) psegAddrInput.placeholder = 'Default: 0xD0000000 (user code)';
                        if (dsegAddrInput) dsegAddrInput.placeholder = 'Default: 0xF0000000 (user data)';
                    }
                } else {
                    // MMU Disabled: Hide Mode and Domain, use physical addresses
                    mmuStatusValue.textContent = '✗ Disabled';
                    mmuStatusValue.style.color = '#f44336';
                    mmuStatusHint.textContent = 'Physical memory: 0x00000000-0x00FFFFFF (16MB) - Direct addressing, no domains';
                    mmuStatusBanner.style.backgroundColor = '#ffebee';
                    mmuToggleBtn.textContent = 'Enable MMU';
                    mmuToggleBtn.style.backgroundColor = '#4CAF50';

                    // Hide Mode and Domain fields
                    if (modeRow) modeRow.style.display = 'none';
                    if (domainRow) domainRow.style.display = 'none';

                    // Update placeholders for physical addresses
                    if (psegAddrInput) psegAddrInput.placeholder = 'Physical address (0x00000000-0x003FFFFF)';
                    if (dsegAddrInput) dsegAddrInput.placeholder = 'Physical address (0x00400000-0x007FFFFF)';
                }
            }
        };

        // Update domain when mode changes
        modeSelect.onchange = () => {
            if (modeSelect.value === 'kernel') {
                domainSelect.value = '0';
            } else {
                domainSelect.value = '1';
            }
            updateMmuStatus();
        };

        // Initial MMU status update
        updateMmuStatus();

        // MMU toggle button handler
        mmuToggleBtn.onclick = async () => {
            if (this.module) {
                const mmuEnabled = this.module.ccall('nd500_dbg_mmu_is_enabled_js', 'number', [], []);

                if (mmuEnabled) {
                    // Disable MMU
                    this.module.ccall('nd500_dbg_execute_command_js', 'number', ['string'], ['mmu off']);
                    this.updateStatus('MMU disabled');
                } else {
                    // Enable MMU - run mmusetup
                    this.module.ccall('nd500_dbg_execute_command_js', 'number', ['string'], ['mmusetup']);
                    this.updateStatus('MMU enabled with default configuration');
                }

                // Update UI after toggle
                updateMmuStatus();
                this.updateUI();
            }
        };

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
                    const domain = parseInt(domainSelect.value, 10);
                    const psegAddrInput = document.getElementById('psegAddr').value.trim();
                    const dsegAddrInput = document.getElementById('dsegAddr').value.trim();

                    // Use custom addresses if provided, otherwise use mode defaults
                    // NOTE: These are VIRTUAL addresses - MMU will translate to physical
                    let psegBase = (mode === 'kernel') ? (0x08000000 >>> 0) : (0xD0000000 >>> 0);
                    let dsegBase = (mode === 'kernel') ? (0x00000000 >>> 0) : (0xF0000000 >>> 0);

                    if (psegAddrInput) {
                        psegBase = this.parseHexOrDec(psegAddrInput) >>> 0;
                    }
                    if (dsegAddrInput) {
                        dsegBase = this.parseHexOrDec(dsegAddrInput) >>> 0;
                    }

                    await this.loadSplitViaMemfs(pseg, psegBase, dseg, dsegBase, startPc.value, domain);
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

    openMmuModal(initialTab = 'mmu') {
        if (!this.module) return;

        const modal = document.getElementById('mmuModal');
        const cancelBtn = document.getElementById('mmuCancelBtn');
        const toggleBtn = document.getElementById('mmuToggleBtn');
        const setupBtn = document.getElementById('mmuSetupBtn');
        const translateBtn = document.getElementById('mmuTranslateBtn');
        const pstRefreshBtn = document.getElementById('pstRefreshBtn');
        const pcbRefreshBtn = document.getElementById('pcbRefreshBtn');

        // Setup tab switching
        const tabs = document.querySelectorAll('.mmu-tab');
        const tabContents = document.querySelectorAll('.mmu-tab-content');

        tabs.forEach(tab => {
            tab.onclick = () => {
                const targetTab = tab.dataset.tab;

                // Remove active class from all tabs and contents
                tabs.forEach(t => t.classList.remove('active'));
                tabContents.forEach(tc => tc.classList.remove('active'));

                // Add active class to clicked tab
                tab.classList.add('active');

                // Show corresponding content
                const contentMap = {
                    'mmu': 'mmuTabContent',
                    'pst': 'pstTabContent',
                    'pcb': 'pcbTabContent'
                };

                const content = document.getElementById(contentMap[targetTab]);
                if (content) {
                    content.classList.add('active');

                    // Load data for newly activated tab
                    if (targetTab === 'pst') {
                        this.updatePstTable();
                    } else if (targetTab === 'pcb') {
                        this.updatePcbDomainList();
                    }
                }
            };
        });

        // Update MMU status
        this.updateMmuModal();

        // Cancel button closes modal
        cancelBtn.onclick = () => {
            modal.classList.add('hidden');
        };

        // Toggle MMU on/off
        toggleBtn.onclick = () => {
            try {
                const output = this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], ['mmu']);
                const isEnabled = output.includes('enabled');

                // Toggle state
                const newState = isEnabled ? 'off' : 'on';
                this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], [`mmu ${newState}`]);

                // Update display
                this.updateMmuModal();
                this.updateMmuPanel();
                this.updateStatus(`MMU ${newState === 'on' ? 'enabled' : 'disabled'}`);
            } catch (error) {
                console.error('Error toggling MMU:', error);
                this.updateStatus('Error toggling MMU');
            }
        };

        // Setup demo configuration
        setupBtn.onclick = () => {
            try {
                const output = this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], ['mmusetup']);
                this.updateMmuModal();
                this.updateMmuPanel();
                this.updateStatus('MMU demo configuration loaded');

                // Show output in console if available
                if (window.consoleManager) {
                    window.consoleManager.addLine('> mmusetup', 'command');
                    output.split('\n').forEach(line => {
                        if (line.trim()) window.consoleManager.addLine(line, 'output');
                    });
                }

                // Refresh PST/PCB tabs if they're visible
                this.updatePstTable();
                this.updatePcbDomainList();
            } catch (error) {
                console.error('Error running mmusetup:', error);
                this.updateStatus('Error running mmusetup');
            }
        };

        // Translate address
        translateBtn.onclick = () => {
            const addr = prompt('Enter virtual address to translate (hex):', '0x00000000');
            if (addr) {
                try {
                    const output = this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], [`phyladr ${addr}`]);
                    if (window.consoleManager) {
                        window.consoleManager.show();
                        window.consoleManager.addLine(`> phyladr ${addr}`, 'command');
                        output.split('\n').forEach(line => {
                            if (line.trim()) window.consoleManager.addLine(line, 'output');
                        });
                    }
                } catch (error) {
                    console.error('Error translating address:', error);
                }
            }
        };

        // PST refresh button
        pstRefreshBtn.onclick = () => {
            this.updatePstTable();
            this.updateMmuModal();
            this.updateStatus('PST table refreshed');
        };

        // PCB refresh button
        pcbRefreshBtn.onclick = () => {
            this.updatePcbDomainList();
            this.updateMmuModal();
            this.updateStatus('PCB table refreshed');
        };

        // Setup PST filter handlers
        const pstSearchInput = document.getElementById('pstSearchInput');
        const pstModeRadios = document.querySelectorAll('input[name="pstMode"]');

        pstSearchInput.oninput = () => {
            this.updatePstTable();
        };

        pstModeRadios.forEach(radio => {
            radio.onchange = () => {
                this.updatePstTable();
            };
        });

        // Setup PCB filter handlers
        const pcbSearchInput = document.getElementById('pcbSearchInput');
        pcbSearchInput.oninput = () => {
            this.updatePcbDomainList();
        };

        // Add click handlers for register editing
        document.querySelectorAll('.mmu-reg-item').forEach(item => {
            item.addEventListener('click', (e) => {
                const regName = e.currentTarget.dataset.reg;
                const currentValue = document.getElementById(`reg${regName}`).textContent;
                const newValue = prompt(`Enter new value for ${regName} (hex):`, currentValue);

                if (newValue !== null && newValue.trim()) {
                    let value;
                    if (newValue.startsWith('0x') || newValue.startsWith('0X')) {
                        value = parseInt(newValue, 16);
                    } else {
                        value = parseInt(newValue, 10);
                    }

                    if (!isNaN(value)) {
                        try {
                            this.module.ccall('nd500_dbg_set_reg_js', null, ['string', 'number'], [regName, value]);
                            this.updateMmuModal();
                            this.updateStatus(`${regName} set to 0x${value.toString(16).padStart(8,'0')}`);
                        } catch (error) {
                            console.error('Error setting register:', error);
                            this.updateStatus('Error setting register');
                        }
                    } else {
                        alert('Invalid value. Please enter a valid hex (0x...) or decimal number.');
                    }
                }
            });
        });

        // Activate initial tab
        tabs.forEach(t => t.classList.remove('active'));
        tabContents.forEach(tc => tc.classList.remove('active'));

        const initialTabBtn = document.querySelector(`[data-tab="${initialTab}"]`);
        if (initialTabBtn) {
            initialTabBtn.classList.add('active');
            const contentMap = {
                'mmu': 'mmuTabContent',
                'pst': 'pstTabContent',
                'pcb': 'pcbTabContent'
            };
            const content = document.getElementById(contentMap[initialTab]);
            if (content) {
                content.classList.add('active');

                // Load initial data if not MMU tab
                if (initialTab === 'pst') {
                    this.updatePstTable();
                } else if (initialTab === 'pcb') {
                    this.updatePcbDomainList();
                }
            }
        }

        modal.classList.remove('hidden');
    }

    updateMmuModal() {
        if (!this.module) return;

        try {
            // Get MMU status via command
            const output = this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], ['showmmu']);
            const isEnabled = output.includes('enabled');

            // Update state display
            const stateElem = document.getElementById('mmuState');
            const toggleBtn = document.getElementById('mmuToggleBtn');

            if (isEnabled) {
                stateElem.textContent = 'enabled';
                stateElem.className = 'mmu-value enabled';
                toggleBtn.textContent = 'Disable';
                toggleBtn.className = 'mmu-toggle-btn disable';
            } else {
                stateElem.textContent = 'disabled';
                stateElem.className = 'mmu-value disabled';
                toggleBtn.textContent = 'Enable';
                toggleBtn.className = 'mmu-toggle-btn';
            }

            // Parse PST count from output
            const pstMatch = output.match(/PST: (\d+) configured entries \(of (\d+) max\)/);
            if (pstMatch) {
                document.getElementById('mmuPstCount').textContent =
                    `${pstMatch[1]} configured (of ${pstMatch[2]} max)`;
            }

            // Parse PCB count from output
            const pcbMatch = output.match(/PCB: (\d+) domains with (\d+) segments/);
            if (pcbMatch) {
                document.getElementById('mmuPcbCount').textContent =
                    `${pcbMatch[1]} domains with ${pcbMatch[2]} segments`;
            }

            // Get registers from JSON
            const json = this.module.ccall('nd500_dbg_regs_json', 'string', [], []);
            const regs = JSON.parse(json);

            // Update MMU registers
            if (regs.PSTP !== undefined) {
                document.getElementById('regPSTP').textContent = '0x' + regs.PSTP.toString(16).padStart(8,'0').toUpperCase();
            }
            if (regs.DITBASE !== undefined) {
                document.getElementById('regDITBASE').textContent = '0x' + regs.DITBASE.toString(16).padStart(8,'0').toUpperCase();
            }
            if (regs.CED !== undefined) {
                document.getElementById('regCED').textContent = '0x' + regs.CED.toString(16).padStart(8,'0').toUpperCase();
            }
            if (regs.CAD !== undefined) {
                document.getElementById('regCAD').textContent = '0x' + regs.CAD.toString(16).padStart(8,'0').toUpperCase();
            }
            if (regs.PS !== undefined) {
                document.getElementById('regPS').textContent = '0x' + regs.PS.toString(16).padStart(8,'0').toUpperCase();
            }
        } catch (error) {
            console.error('Error updating MMU modal:', error);
        }
    }

    updateMmuPanel() {
        if (!this.module) return;

        try {
            // Get MMU status
            const output = this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], ['showmmu']);
            const isEnabled = output.includes('enabled');

            // Parse counts
            const pstMatch = output.match(/PST: (\d+) configured entries/);
            const pcbMatch = output.match(/PCB: (\d+) domains with (\d+) segments/);

            const pstCount = pstMatch ? pstMatch[1] : '0';
            const pcbDomains = pcbMatch ? pcbMatch[1] : '0';
            const pcbSegments = pcbMatch ? pcbMatch[2] : '0';

            // Create inline status display
            const statusClass = isEnabled ? 'enabled' : 'disabled';
            const statusText = isEnabled ? 'Enabled' : 'Disabled';

            const html = `
                <div class="mmu-status-inline">
                    <div class="mmu-status-inline-row">
                        <span class="mmu-status-inline-label">State:</span>
                        <span class="mmu-status-inline-value ${statusClass}">${statusText}</span>
                    </div>
                    <div class="mmu-status-inline-row">
                        <span class="mmu-status-inline-label">PST:</span>
                        <span class="mmu-status-inline-value">${pstCount} entries</span>
                    </div>
                    <div class="mmu-status-inline-row">
                        <span class="mmu-status-inline-label">PCB:</span>
                        <span class="mmu-status-inline-value">${pcbDomains} domains, ${pcbSegments} segs</span>
                    </div>
                </div>
            `;

            document.getElementById('mmu-content').innerHTML = html;
        } catch (error) {
            console.error('Error updating MMU panel:', error);
            document.getElementById('mmu-content').innerHTML =
                '<div class="mmu-status-inline-row">MMU status unavailable</div>';
        }
    }

    updatePstTable() {
        if (!this.module) return;

        try {
            // Get listpst output
            const output = this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], ['listpst']);

            // Parse PST entries from output
            const entries = [];
            const lines = output.split('\n');

            for (const line of lines) {
                // Match lines like: " 100  AZI   0x1000  0x00800000"
                const match = line.match(/^\s*(\d+)\s+(AZI|ASI|ADI)\s+0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)/);
                if (match) {
                    entries.push({
                        psn: parseInt(match[1]),
                        mode: match[2],
                        pfn: parseInt(match[3], 16),
                        physAddr: parseInt(match[4], 16)
                    });
                }
            }

            // Apply filters
            const modeFilter = document.querySelector('input[name="pstMode"]:checked')?.value || 'all';
            const searchText = document.getElementById('pstSearchInput').value.toLowerCase().trim();

            let filtered = entries;

            // Filter by mode
            if (modeFilter !== 'all') {
                filtered = filtered.filter(e => e.mode === modeFilter);
            }

            // Filter by search (PSN)
            if (searchText) {
                filtered = filtered.filter(e => e.psn.toString().includes(searchText));
            }

            // Update stats
            document.getElementById('pstTableStats').textContent =
                `${filtered.length} entries shown` + (filtered.length !== entries.length ? ` (of ${entries.length} total)` : '');

            // Render table
            const tbody = document.getElementById('pst-table-body');

            if (filtered.length === 0) {
                tbody.innerHTML = `<tr><td colspan="5" class="pst-table-empty">No PST entries found. Use 'mmusetup' to create a demo configuration.</td></tr>`;
            } else {
                let html = '';
                filtered.forEach(entry => {
                    const modeClass = entry.mode.toLowerCase();
                    html += `<tr onclick="nd500Debugger.openPstEditModal(${entry.psn}, '${entry.mode}', ${entry.pfn})">
                        <td>${entry.psn}</td>
                        <td><span class="pst-mode-badge ${modeClass}">${entry.mode}</span></td>
                        <td>0x${entry.pfn.toString(16).padStart(4,'0').toUpperCase()}</td>
                        <td>0x${entry.physAddr.toString(16).padStart(8,'0').toUpperCase()}</td>
                        <td><button class="pst-action-btn" onclick="event.stopPropagation(); nd500Debugger.openPstEditModal(${entry.psn}, '${entry.mode}', ${entry.pfn});">Edit</button></td>
                    </tr>`;
                });
                tbody.innerHTML = html;
            }
        } catch (error) {
            console.error('Error updating PST table:', error);
            const tbody = document.getElementById('pst-table-body');
            tbody.innerHTML = `<tr><td colspan="5" class="pst-table-empty">Error loading PST entries</td></tr>`;
        }
    }

    openPstEditModal(psn, mode, pfn) {
        if (!this.module) return;

        const modal = document.getElementById('pstEditModal');
        const psnSpan = document.getElementById('pstEditPsn');
        const psnInput = document.getElementById('pstEditPsnInput');
        const modeSelect = document.getElementById('pstEditMode');
        const pfnInput = document.getElementById('pstEditPfn');
        const physAddrInput = document.getElementById('pstEditPhysAddr');
        const cancelBtn = document.getElementById('pstEditCancelBtn');
        const saveBtn = document.getElementById('pstEditSaveBtn');

        // Populate form
        psnSpan.textContent = psn;
        psnInput.value = psn;

        // Map mode string to value
        const modeMap = { 'AZI': 0, 'ASI': 1, 'ADI': 2 };
        modeSelect.value = modeMap[mode] || 0;

        pfnInput.value = '0x' + pfn.toString(16).padStart(4, '0').toUpperCase();

        // Calculate and show physical address
        const updatePhysAddr = () => {
            try {
                const pfnValue = pfnInput.value.startsWith('0x') ?
                    parseInt(pfnInput.value, 16) : parseInt(pfnInput.value, 10);
                const physAddr = pfnValue << 11; // PGSHIFT = 11
                physAddrInput.value = '0x' + physAddr.toString(16).padStart(8, '0').toUpperCase();
            } catch (e) {
                physAddrInput.value = 'Invalid PFN';
            }
        };

        updatePhysAddr();

        // Update physical address when PFN changes
        pfnInput.oninput = updatePhysAddr;

        // Cancel button
        cancelBtn.onclick = () => {
            modal.classList.add('hidden');
        };

        // Save button
        saveBtn.onclick = () => {
            try {
                const newMode = modeSelect.value;
                const newPfnStr = pfnInput.value.trim();
                const newPfn = newPfnStr.startsWith('0x') ?
                    parseInt(newPfnStr, 16) : parseInt(newPfnStr, 10);

                if (isNaN(newPfn)) {
                    alert('Invalid PFN value. Please enter a valid hex (0x...) or decimal number.');
                    return;
                }

                // Execute setpst command (if available) or manual register writes
                // For now, we'll use a command approach
                const cmd = `setpst ${psn} ${newMode} ${newPfn}`;
                console.log('Executing PST update:', cmd);

                // Note: setpst command may not exist, so we'll provide user feedback
                // In a full implementation, this would call the backend command
                this.updateStatus(`PST[${psn}] would be set to mode=${newMode}, pfn=0x${newPfn.toString(16)} (command not yet implemented in backend)`);

                modal.classList.add('hidden');

                // Refresh PST table
                this.updatePstTable();
            } catch (error) {
                console.error('Error saving PST entry:', error);
                alert('Error saving PST entry: ' + error.message);
            }
        };

        modal.classList.remove('hidden');
    }

    updatePcbDomainList() {
        if (!this.module) return;

        try {
            // Get listpcb output
            const output = this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], ['listpcb']);

            // Parse PCB domains from output
            const domains = [];
            const lines = output.split('\n');
            let currentDomain = null;

            for (const line of lines) {
                // Match domain headers like: "Domain 0:"
                const domainMatch = line.match(/^Domain (\d+):/);
                if (domainMatch) {
                    if (currentDomain) {
                        domains.push(currentDomain);
                    }
                    currentDomain = {
                        domain: parseInt(domainMatch[1]),
                        segments: []
                    };
                    continue;
                }

                // Match segment lines like: "    0  0064   0064   P:PSN=100 D:PSN=100"
                if (currentDomain) {
                    const segMatch = line.match(/^\s+(\d+)\s+([0-9A-Fa-f]{4})\s+([0-9A-Fa-f]{4})\s+(.+)$/);
                    if (segMatch) {
                        const segment = parseInt(segMatch[1]);
                        const progCap = parseInt(segMatch[2], 16);
                        const dataCap = parseInt(segMatch[3], 16);
                        const description = segMatch[4];

                        // Decode capabilities
                        const progInfo = this.decodeProgCapability(progCap);
                        const dataInfo = this.decodeDataCapability(dataCap);

                        currentDomain.segments.push({
                            segment,
                            progCap,
                            dataCap,
                            progInfo,
                            dataInfo,
                            description
                        });
                    }
                }
            }

            // Push last domain
            if (currentDomain) {
                domains.push(currentDomain);
            }

            // Apply search filter
            const searchText = document.getElementById('pcbSearchInput').value.toLowerCase().trim();
            let filtered = domains;

            if (searchText) {
                filtered = filtered.filter(d => d.domain.toString().includes(searchText));
            }

            // Update stats
            const totalSegments = filtered.reduce((sum, d) => sum + d.segments.length, 0);
            document.getElementById('pcbTableStats').textContent =
                `${filtered.length} domains shown with ${totalSegments} segments` +
                (filtered.length !== domains.length ? ` (of ${domains.length} total domains)` : '');

            // Render domain list
            const container = document.getElementById('pcb-domain-list');

            if (filtered.length === 0) {
                container.innerHTML = `<div class="pcb-empty">No PCB domains found. Use 'mmusetup' to create a demo configuration.</div>`;
            } else {
                let html = '';
                filtered.forEach(domain => {
                    html += this.renderPcbDomain(domain);
                });
                container.innerHTML = html;

                // Add click handlers for expand/collapse
                document.querySelectorAll('.pcb-domain-header').forEach(header => {
                    header.onclick = () => {
                        const domainId = header.dataset.domain;
                        this.togglePcbDomain(domainId);
                    };
                });
            }
        } catch (error) {
            console.error('Error updating PCB domain list:', error);
            const container = document.getElementById('pcb-domain-list');
            container.innerHTML = `<div class="pcb-empty">Error loading PCB domains</div>`;
        }
    }

    renderPcbDomain(domain) {
        let html = `
            <div class="pcb-domain" data-domain="${domain.domain}">
                <div class="pcb-domain-header" data-domain="${domain.domain}">
                    <span class="pcb-expand-arrow">▶</span>
                    <span class="pcb-domain-title">Domain ${domain.domain}</span>
                    <span class="pcb-domain-count">${domain.segments.length} segments</span>
                </div>
                <div class="pcb-segments" data-domain="${domain.domain}">
                    <table class="pcb-segments-table">
                        <thead>
                            <tr>
                                <th>Seg</th>
                                <th>Prog Cap</th>
                                <th>Data Cap</th>
                                <th>Flags</th>
                                <th>Description</th>
                                <th>Actions</th>
                            </tr>
                        </thead>
                        <tbody>`;

        domain.segments.forEach(seg => {
            // Build flags display
            let flags = [];
            if (seg.progInfo.dir) flags.push('<span class="pcb-flag dir">DIR</span>');
            if (seg.dataInfo.wrp) flags.push('<span class="pcb-flag wrp">WRP</span>');
            if (seg.dataInfo.pac) flags.push('<span class="pcb-flag pac">PAC</span>');
            const flagsHtml = flags.length > 0 ? flags.join(' ') : '<span class="pcb-no-flags">-</span>';

            html += `
                <tr>
                    <td>${seg.segment}</td>
                    <td>0x${seg.progCap.toString(16).padStart(4,'0').toUpperCase()}</td>
                    <td>0x${seg.dataCap.toString(16).padStart(4,'0').toUpperCase()}</td>
                    <td>${flagsHtml}</td>
                    <td class="pcb-description">${this.escapeHtml(seg.description)}</td>
                    <td><button class="pcb-action-btn" onclick="event.stopPropagation(); nd500Debugger.openPcbEditModal(${domain.domain}, ${seg.segment}, ${seg.progCap}, ${seg.dataCap});">Edit</button></td>
                </tr>`;
        });

        html += `
                        </tbody>
                    </table>
                </div>
            </div>`;

        return html;
    }

    togglePcbDomain(domainId) {
        const header = document.querySelector(`.pcb-domain-header[data-domain="${domainId}"]`);
        const segments = document.querySelector(`.pcb-segments[data-domain="${domainId}"]`);
        const arrow = header.querySelector('.pcb-expand-arrow');

        if (segments.classList.contains('expanded')) {
            segments.classList.remove('expanded');
            header.classList.remove('expanded');
            arrow.textContent = '▶';
        } else {
            segments.classList.add('expanded');
            header.classList.add('expanded');
            arrow.textContent = '▼';
        }
    }

    decodeProgCapability(cap) {
        // Program capability: PSN (bits 0-12), DIR (bit 15)
        const psn = cap & 0x1FFF;
        const dir = (cap & 0x8000) !== 0;
        return { psn, dir };
    }

    decodeDataCapability(cap) {
        // Data capability: PSN (bits 0-12), WRP (bit 14), PAC (bit 15)
        const psn = cap & 0x1FFF;
        const wrp = (cap & 0x4000) !== 0;
        const pac = (cap & 0x8000) !== 0;
        return { psn, wrp, pac };
    }

    openPcbEditModal(domain, segment, progCap, dataCap) {
        if (!this.module) return;

        const modal = document.getElementById('pcbEditModal');
        const domainSpan = document.getElementById('pcbEditDomain');
        const segmentSpan = document.getElementById('pcbEditSegment');
        const domainInput = document.getElementById('pcbEditDomainInput');
        const segmentInput = document.getElementById('pcbEditSegmentInput');
        const progPsnInput = document.getElementById('pcbEditProgPsn');
        const progDirCheck = document.getElementById('pcbEditProgDir');
        const dataPsnInput = document.getElementById('pcbEditDataPsn');
        const dataWrpCheck = document.getElementById('pcbEditDataWrp');
        const dataPacCheck = document.getElementById('pcbEditDataPac');
        const cancelBtn = document.getElementById('pcbEditCancelBtn');
        const saveBtn = document.getElementById('pcbEditSaveBtn');

        // Populate form
        domainSpan.textContent = domain;
        segmentSpan.textContent = segment;
        domainInput.value = domain;
        segmentInput.value = segment;

        // Decode capabilities
        const progInfo = this.decodeProgCapability(progCap);
        const dataInfo = this.decodeDataCapability(dataCap);

        // Populate program capability
        progPsnInput.value = progInfo.psn === 0 ? '' : progInfo.psn.toString();
        progDirCheck.checked = progInfo.dir;

        // Populate data capability
        dataPsnInput.value = dataInfo.psn === 0 ? '' : dataInfo.psn.toString();
        dataWrpCheck.checked = dataInfo.wrp;
        dataPacCheck.checked = dataInfo.pac;

        // Cancel button
        cancelBtn.onclick = () => {
            modal.classList.add('hidden');
        };

        // Save button
        saveBtn.onclick = () => {
            try {
                // Encode program capability
                let newProgCap = 0;
                const progPsn = progPsnInput.value.trim();
                if (progPsn) {
                    newProgCap = parseInt(progPsn, 10) & 0x1FFF;
                }
                if (progDirCheck.checked) {
                    newProgCap |= 0x8000;
                }

                // Encode data capability
                let newDataCap = 0;
                const dataPsn = dataPsnInput.value.trim();
                if (dataPsn) {
                    newDataCap = parseInt(dataPsn, 10) & 0x1FFF;
                }
                if (dataWrpCheck.checked) {
                    newDataCap |= 0x4000;
                }
                if (dataPacCheck.checked) {
                    newDataCap |= 0x8000;
                }

                // Execute setpcb command (if available)
                const cmd = `setpcb ${domain} ${segment} 0x${newProgCap.toString(16)} 0x${newDataCap.toString(16)}`;
                console.log('Executing PCB update:', cmd);

                // Note: setpcb command may not exist, so we'll provide user feedback
                this.updateStatus(`PCB[${domain}][${segment}] would be set to prog=0x${newProgCap.toString(16)}, data=0x${newDataCap.toString(16)} (command not yet implemented in backend)`);

                modal.classList.add('hidden');

                // Refresh PCB viewer
                this.updatePcbDomainList();
            } catch (error) {
                console.error('Error saving PCB entry:', error);
                alert('Error saving PCB entry: ' + error.message);
            }
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

    async loadSplitViaMemfs(psegFile, psegBase, dsegFile, dsegBase, startPcValue, domain = 0) {
        console.log('=== loadSplitViaMemfs START ===');
        console.log('PSEG file:', psegFile ? psegFile.name : 'none', 'size:', psegFile ? psegFile.size : 0);
        console.log('PSEG base:', '0x' + psegBase.toString(16));
        console.log('DSEG file:', dsegFile ? dsegFile.name : 'none', 'size:', dsegFile ? dsegFile.size : 0);
        console.log('DSEG base:', '0x' + dsegBase.toString(16));
        console.log('Start PC:', startPcValue || '(default)');
        console.log('Domain:', domain);

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

        // Set the CED (Current Executing Domain) register
        console.log('Setting CED register to domain:', domain);
        this.module.ccall('nd500_dbg_set_reg_js', 'number', ['string', 'number'], ['CED', domain]);
        console.log('Setting CAD register to domain:', domain);
        this.module.ccall('nd500_dbg_set_reg_js', 'number', ['string', 'number'], ['CAD', domain]);

        const segNames = [];
        if (psegFile) segNames.push(`PSEG@0x${psegBase.toString(16).toUpperCase()}`);
        if (dsegFile) segNames.push(`DSEG@0x${dsegBase.toString(16).toUpperCase()}`);
        this.updateStatus(`Loaded: ${segNames.join(', ')} to domain ${domain}`);

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

                // Automatically run mmusetup to initialize MMU with demo configuration
                try {
                    console.log('Running automatic mmusetup...');
                    const output = this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], ['mmusetup']);
                    console.log('mmusetup completed:', output);
                    this.updateStatus('Demo kernel loaded with MMU configuration');
                    this.updateMmuPanel();
                } catch (mmuError) {
                    console.warn('Could not run automatic mmusetup:', mmuError);
                }

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
        this.updateMmuPanel();
    }

    updateRegisters() {
        if (!this.module) return;

        try {
            const json = this.module.ccall('nd500_dbg_regs_json', 'string', [], []);
            const regs = JSON.parse(json);
            this.currentPC = regs.PC;

            // Build CPU registers section (general purpose registers only)
            let html = `
                <div class="reg-section-title">CPU Registers</div>
                <div class="reg-item" data-reg="PC" data-value="${regs.PC}" title="Program Counter">PC: 0x${regs.PC.toString(16).padStart(8,'0')}</div>
                ${regs.I.map((v,i) => `<div class="reg-item" data-reg="I${i+1}" data-value="${v}" title="Index Register ${i+1}">I${i+1}: 0x${v.toString(16).padStart(8,'0')}</div>`).join('')}
                ${regs.A.map((v,i) => `<div class="reg-item" data-reg="A${i+1}" data-value="${v}" title="Address Register ${i+1}">A${i+1}: 0x${v.toString(16).padStart(8,'0')}</div>`).join('')}
                ${regs.E.map((v,i) => `<div class="reg-item" data-reg="E${i+1}" data-value="${v}" title="Extension Register ${i+1}">E${i+1}: 0x${v.toString(16).padStart(8,'0')}</div>`).join('')}
                <div class="reg-item" data-reg="L" data-value="${regs.L}" title="Level Register">L: 0x${regs.L.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="B" data-value="${regs.B}" title="Base Register">B: 0x${regs.B.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="R" data-value="${regs.R}" title="Return Address">R: 0x${regs.R.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="TOS" data-value="${regs.TOS}" title="Top of Stack">TOS: 0x${regs.TOS.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="LL" data-value="${regs.LL}" title="Lower Limit">LL: 0x${regs.LL.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="HL" data-value="${regs.HL}" title="Higher Limit">HL: 0x${regs.HL.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="THA" data-value="${regs.THA}" title="Trap Handler Address">THA: 0x${regs.THA.toString(16).padStart(8,'0')}</div>
            `;

            // Add Flag registers section
            html += `
                <div class="reg-section-title">Flag Registers</div>
                <div class="reg-item" data-reg="FLAGS" data-value="${regs.FLAGS}" title="CPU Status Flags">FLAGS: 0x${regs.FLAGS.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="ST1" data-value="${regs.ST1}" title="Status Register 1 (Trap Bits 11-31)">ST1: 0x${regs.ST1.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="ST2" data-value="${regs.ST2}" title="Status Register 2 (Trap Bits 0-10)">ST2: 0x${regs.ST2.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="OTE1" data-value="${regs.OTE1}" title="Own Trap Enable 1 (Bits 11-31)">OTE1: 0x${regs.OTE1.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="OTE2" data-value="${regs.OTE2}" title="Own Trap Enable 2 (Bits 0-10)">OTE2: 0x${regs.OTE2.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="CTE1" data-value="${regs.CTE1}" title="Child Trap Enable 1 (Bits 11-31)">CTE1: 0x${regs.CTE1.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="CTE2" data-value="${regs.CTE2}" title="Child Trap Enable 2 (Bits 0-10)">CTE2: 0x${regs.CTE2.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="MTE1" data-value="${regs.MTE1}" title="Mother Trap Enable 1 (Bits 11-31)">MTE1: 0x${regs.MTE1.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="MTE2" data-value="${regs.MTE2}" title="Mother Trap Enable 2 (Bits 0-10)">MTE2: 0x${regs.MTE2.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="TEMM1" data-value="${regs.TEMM1}" title="Trap Enable Mod Mask 1 (Bits 11-31)">TEMM1: 0x${regs.TEMM1.toString(16).padStart(8,'0')}</div>
                <div class="reg-item" data-reg="TEMM2" data-value="${regs.TEMM2}" title="Trap Enable Mod Mask 2 (Bits 0-10)">TEMM2: 0x${regs.TEMM2.toString(16).padStart(8,'0')}</div>
            `;

            // Add MMU registers section if available
            if (regs.PSTP !== undefined) {
                html += `
                <div class="reg-section-title mmu-section">MMU Registers</div>
                <div class="reg-item mmu-reg" data-reg="PSTP" data-value="${regs.PSTP}" title="Physical Segment Table Pointer">PSTP: 0x${regs.PSTP.toString(16).padStart(8,'0')}</div>
                <div class="reg-item mmu-reg" data-reg="DITBASE" data-value="${regs.DITBASE}" title="Domain Information Table Base">DITBASE: 0x${regs.DITBASE.toString(16).padStart(8,'0')}</div>
                <div class="reg-item mmu-reg" data-reg="CED" data-value="${regs.CED}" title="Current Executing Domain">CED: 0x${regs.CED.toString(16).padStart(8,'0')}</div>
                <div class="reg-item mmu-reg" data-reg="CAD" data-value="${regs.CAD}" title="Current Alternative Domain">CAD: 0x${regs.CAD.toString(16).padStart(8,'0')}</div>
                <div class="reg-item mmu-reg" data-reg="PS" data-value="${regs.PS}" title="Process Segment">PS: 0x${regs.PS.toString(16).padStart(8,'0')}</div>
                `;
            }

            document.getElementById('regs-content').innerHTML = html;
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

    setMemoryAddress(address) {
        const addrInput = document.getElementById('memAddr');
        if (addrInput) {
            addrInput.value = address.toString(16).padStart(8, '0');
            this.viewMemory();
        }
    }

    openBitEditor(regName, currentValue) {
        const modal = document.getElementById('bitEditorModal');
        const grid = document.getElementById('bitEditorGrid');
        const regNameSpan = document.getElementById('bitEditorRegName');
        const currentValueSpan = document.getElementById('bitEditorCurrentValue');
        const preview = document.getElementById('bitEditorPreview');
        const cancelBtn = document.getElementById('bitEditorCancelBtn');
        const applyBtn = document.getElementById('bitEditorApplyBtn');

        // Register descriptions
        const REGISTER_DESCRIPTIONS = {
            'ST1': 'Status Register Upper',
            'ST2': 'Status Register Lower',
            'TOS': 'Top of Stack',
            'LL': 'Lower Limit',
            'HL': 'Higher Limit',
            'THA': 'Trap Handler Address',
            'OTE1': 'Trap Enable Upper',
            'OTE2': 'Trap Enable Lower',
            'CTE1': 'Calculated Trap Enable Upper',
            'CTE2': 'Calculated Trap Enable Lower',
            'MTE1': 'Monitor Trap Enable Upper',
            'MTE2': 'Monitor Trap Enable Lower',
            'TEMM1': 'Trap Emulation Mask Upper',
            'TEMM2': 'Trap Emulation Mask Lower',
            'FLAGS': 'CPU Flags'
        };

        // Get bit definitions for this register
        const definition = this.getBitDefinition(regName);
        if (!definition) {
            alert(`No bit definition found for register ${regName}`);
            return;
        }

        // Set header with description
        const description = REGISTER_DESCRIPTIONS[regName];
        regNameSpan.textContent = description ? `${regName} (${description})` : regName;
        currentValueSpan.textContent = '0x' + (currentValue >>> 0).toString(16).padStart(definition.bits / 4, '0').toUpperCase();

        // Build bit grid (MSB first - left to right)
        let html = '';
        for (let bit = definition.bits - 1; bit >= 0; bit--) {
            const isSet = (currentValue & (1 << bit)) !== 0;
            const label = definition.labels[bit] || '-';
            const description = definition.descriptions ? (definition.descriptions[bit] || '') : '';
            const titleText = description || label || `Bit ${bit}`;

            html += `
                <div class="bit-item ${isSet ? 'active' : ''}" data-bit="${bit}" data-description="${this.escapeHtml(description)}" title="${this.escapeHtml(titleText)}">
                    <div class="bit-label">${this.escapeHtml(label)}</div>
                    <div class="bit-toggle">${isSet ? '1' : '0'}</div>
                    <div class="bit-number">${bit}</div>
                </div>
            `;
        }

        grid.innerHTML = html;

        // Get description panel
        const descPanel = document.getElementById('bitEditorHoverInfo');

        // Add click and hover handlers for bit toggles
        document.querySelectorAll('.bit-item').forEach(item => {
            // Click to toggle
            item.onclick = () => {
                item.classList.toggle('active');
                const toggle = item.querySelector('.bit-toggle');
                toggle.textContent = item.classList.contains('active') ? '1' : '0';
                this.updateBitPreview();
            };

            // Hover to show description
            item.addEventListener('mouseenter', () => {
                const desc = item.dataset.description;
                if (desc && desc.trim()) {
                    descPanel.textContent = desc;
                    descPanel.style.opacity = '1';
                } else {
                    const bitNum = item.dataset.bit;
                    const label = item.querySelector('.bit-label').textContent;
                    descPanel.textContent = `Bit ${bitNum}${label !== '-' ? ' (' + label + ')' : ''} - No description available`;
                    descPanel.style.opacity = '1';
                }
            });

            // Clear description on mouse leave
            item.addEventListener('mouseleave', () => {
                descPanel.textContent = 'Hover over a bit to see its description';
                descPanel.style.opacity = '0.6';
            });
        });

        // Initial preview update
        this.updateBitPreview();

        // Cancel button
        cancelBtn.onclick = () => {
            modal.classList.add('hidden');
        };

        // Apply button
        applyBtn.onclick = () => {
            const newValue = this.updateBitPreview();

            try {
                this.module.ccall('nd500_dbg_set_reg_js', null, ['string', 'number'], [regName, newValue]);
                // Ensure unsigned conversion for status message
                this.updateStatus(`${regName} set to 0x${(newValue >>> 0).toString(16).padStart(8,'0').toUpperCase()}`);

                // If PC was changed, update disassembly
                if (regName === 'PC') {
                    this.updateDisassembly();
                }

                // Update all registers to reflect the change
                this.updateRegisters();

                modal.classList.add('hidden');
            } catch (error) {
                console.error('Error setting register:', error);
                this.updateStatus('Error setting register');
                alert('Error setting register: ' + error.message);
            }
        };

        modal.classList.remove('hidden');
    }

    updateBitPreview() {
        let value = 0;
        document.querySelectorAll('.bit-item.active').forEach(item => {
            const bit = parseInt(item.dataset.bit);
            value |= (1 << bit);
        });

        // Convert to unsigned 32-bit to avoid negative hex values
        value = value >>> 0;

        const preview = document.getElementById('bitEditorPreview');
        preview.value = '0x' + value.toString(16).padStart(8, '0').toUpperCase();

        return value;
    }

    getBitDefinition(regName) {
        const BIT_DEFINITIONS = {
            'FLAGS': {
                bits: 32,
                labels: {
                    0: 'C',   // Carry flag
                    1: 'V',   // Overflow flag
                    2: 'Z',   // Zero flag
                    3: 'N',   // Negative flag
                    4: 'X',   // Extend flag
                    5: 'I',   // Interrupt enable
                    6: 'S',   // Supervisor mode
                    7: 'T'    // Trace mode
                },
                descriptions: {
                    0: 'Carry: Set when arithmetic operation produces a carry out of the most significant bit',
                    1: 'Overflow: Set when signed arithmetic operation overflows',
                    2: 'Zero: Set when the result of an operation is zero',
                    3: 'Negative: Set when the result of an operation is negative (MSB=1)',
                    4: 'Extend: Extended carry flag for multi-precision arithmetic',
                    5: 'Interrupt Enable: When set, interrupts are enabled',
                    6: 'Supervisor: When set, CPU is in supervisor (privileged) mode',
                    7: 'Trace: When set, CPU executes in single-step trace mode'
                }
            },
            'ST1': {
                bits: 32,
                labels: {
                    11: 'PE', 12: 'PF', 13: 'PI', 14: 'PD', 15: 'PS',
                    16: 'PO', 17: 'PU', 18: 'PZ', 19: 'PM', 20: 'PK',
                    21: 'PX', 22: 'PN', 23: 'PC', 24: 'PL', 25: 'PW',
                    26: 'PG', 27: 'PV', 28: 'PT', 29: 'PR', 30: 'PA', 31: 'PY'
                },
                descriptions: {
                    11: 'PE: Protected Execute - Trap on execute violation',
                    12: 'PF: Divide by Zero - Trap when dividing by zero',
                    13: 'PI: Illegal Instruction - Trap on invalid opcode',
                    14: 'PD: Illegal Data - Trap on illegal operand',
                    15: 'PS: Instruction Sequence Error - Trap on sequence violation',
                    16: 'PO: Illegal I/O Operation - Trap on invalid I/O access',
                    17: 'PU: Unassigned/Undefined Trap',
                    18: 'PZ: Unassigned/Undefined Trap',
                    19: 'PM: Stack Overflow - Trap when stack grows too large',
                    20: 'PK: Stack Underflow - Trap when stack pops below limit',
                    21: 'PX: Floating Point Exception - Trap on FP error',
                    22: 'PN: Floating Point Underflow - Trap on FP underflow',
                    23: 'PC: Floating Point Overflow - Trap on FP overflow',
                    24: 'PL: Power Fail - Trap on power failure',
                    25: 'PW: Watchdog Timer - Trap on watchdog timeout',
                    26: 'PG: Single Instruction Step - Trap after each instruction',
                    27: 'PV: Breakpoint - Trap on breakpoint hit',
                    28: 'PT: Trace - Trap for instruction tracing',
                    29: 'PR: Branch Taken - Trap when branch is taken',
                    30: 'PA: Call Executed - Trap when subroutine is called',
                    31: 'PY: Return Executed - Trap when returning from subroutine'
                }
            },
            'ST2': {
                bits: 32,
                labels: {
                    0: 'XSE', 1: 'IIC', 2: 'IOS', 3: 'ISE', 4: 'PV',
                    5: 'THM', 6: 'PGF', 7: 'NXM', 8: 'MXM', 9: 'ILL'
                },
                descriptions: {
                    0: 'XSE: External Sequence Error - Trap on external bus error',
                    1: 'IIC: Illegal Instruction Combination - Trap on invalid instruction sequence',
                    2: 'IOS: I/O Status Error - Trap on I/O operation failure',
                    3: 'ISE: Internal Sequence Error - Trap on internal state violation',
                    4: 'PV: Parity/Protect Violation - Trap on memory protection fault',
                    5: 'THM: Too High Memory - Trap on memory access out of bounds',
                    6: 'PGF: Page Fault - Trap when accessing non-resident page',
                    7: 'NXM: Non-Existent Memory - Trap on access to unmapped address',
                    8: 'MXM: Memory Access Violation - Trap on illegal memory operation',
                    9: 'ILL: Illegal Operation - Trap on undefined operation'
                }
            },
            'OTE1': {
                bits: 32,
                labels: {
                    11: 'PE', 12: 'PF', 13: 'PI', 14: 'PD', 15: 'PS',
                    16: 'PO', 17: 'PU', 18: 'PZ', 19: 'PM', 20: 'PK',
                    21: 'PX', 22: 'PN', 23: 'PC', 24: 'PL', 25: 'PW',
                    26: 'PG', 27: 'PV', 28: 'PT', 29: 'PR', 30: 'PA', 31: 'PY'
                },
                descriptions: {
                    11: 'PE: Enable trap on Protected Execute violation',
                    12: 'PF: Enable trap on Divide by Zero',
                    13: 'PI: Enable trap on Illegal Instruction',
                    14: 'PD: Enable trap on Illegal Data',
                    15: 'PS: Enable trap on Instruction Sequence Error',
                    16: 'PO: Enable trap on Illegal I/O Operation',
                    17: 'PU: Enable Unassigned Trap',
                    18: 'PZ: Enable Unassigned Trap',
                    19: 'PM: Enable trap on Stack Overflow',
                    20: 'PK: Enable trap on Stack Underflow',
                    21: 'PX: Enable trap on Floating Point Exception',
                    22: 'PN: Enable trap on Floating Point Underflow',
                    23: 'PC: Enable trap on Floating Point Overflow',
                    24: 'PL: Enable trap on Power Fail',
                    25: 'PW: Enable trap on Watchdog Timer',
                    26: 'PG: Enable trap on Single Instruction Step',
                    27: 'PV: Enable trap on Breakpoint',
                    28: 'PT: Enable trap for Trace',
                    29: 'PR: Enable trap on Branch Taken',
                    30: 'PA: Enable trap on Call Executed',
                    31: 'PY: Enable trap on Return Executed'
                }
            },
            'OTE2': {
                bits: 32,
                labels: {
                    0: 'XSE', 1: 'IIC', 2: 'IOS', 3: 'ISE', 4: 'PV',
                    5: 'THM', 6: 'PGF', 7: 'NXM', 8: 'MXM', 9: 'ILL'
                },
                descriptions: {
                    0: 'XSE: Enable trap on External Sequence Error',
                    1: 'IIC: Enable trap on Illegal Instruction Combination',
                    2: 'IOS: Enable trap on I/O Status Error',
                    3: 'ISE: Enable trap on Internal Sequence Error',
                    4: 'PV: Enable trap on Parity/Protect Violation',
                    5: 'THM: Enable trap on Too High Memory',
                    6: 'PGF: Enable trap on Page Fault',
                    7: 'NXM: Enable trap on Non-Existent Memory',
                    8: 'MXM: Enable trap on Memory Access Violation',
                    9: 'ILL: Enable trap on Illegal Operation'
                }
            },
            'CTE1': {
                bits: 32,
                labels: {
                    11: 'PE', 12: 'PF', 13: 'PI', 14: 'PD', 15: 'PS',
                    16: 'PO', 17: 'PU', 18: 'PZ', 19: 'PM', 20: 'PK',
                    21: 'PX', 22: 'PN', 23: 'PC', 24: 'PL', 25: 'PW',
                    26: 'PG', 27: 'PV', 28: 'PT', 29: 'PR', 30: 'PA', 31: 'PY'
                },
                descriptions: {
                    11: 'PE: Child process - enable trap on Protected Execute',
                    12: 'PF: Child process - enable trap on Divide by Zero',
                    13: 'PI: Child process - enable trap on Illegal Instruction',
                    14: 'PD: Child process - enable trap on Illegal Data',
                    15: 'PS: Child process - enable trap on Instruction Sequence Error',
                    16: 'PO: Child process - enable trap on Illegal I/O',
                    17: 'PU: Child process - enable Unassigned Trap',
                    18: 'PZ: Child process - enable Unassigned Trap',
                    19: 'PM: Child process - enable trap on Stack Overflow',
                    20: 'PK: Child process - enable trap on Stack Underflow',
                    21: 'PX: Child process - enable trap on FP Exception',
                    22: 'PN: Child process - enable trap on FP Underflow',
                    23: 'PC: Child process - enable trap on FP Overflow',
                    24: 'PL: Child process - enable trap on Power Fail',
                    25: 'PW: Child process - enable trap on Watchdog',
                    26: 'PG: Child process - enable trap on Single Step',
                    27: 'PV: Child process - enable trap on Breakpoint',
                    28: 'PT: Child process - enable trap for Trace',
                    29: 'PR: Child process - enable trap on Branch',
                    30: 'PA: Child process - enable trap on Call',
                    31: 'PY: Child process - enable trap on Return'
                }
            },
            'CTE2': {
                bits: 32,
                labels: {
                    0: 'XSE', 1: 'IIC', 2: 'IOS', 3: 'ISE', 4: 'PV',
                    5: 'THM', 6: 'PGF', 7: 'NXM', 8: 'MXM', 9: 'ILL'
                },
                descriptions: {
                    0: 'XSE: Child - enable trap on External Sequence Error',
                    1: 'IIC: Child - enable trap on Illegal Instruction Combination',
                    2: 'IOS: Child - enable trap on I/O Status Error',
                    3: 'ISE: Child - enable trap on Internal Sequence Error',
                    4: 'PV: Child - enable trap on Parity/Protect Violation',
                    5: 'THM: Child - enable trap on Too High Memory',
                    6: 'PGF: Child - enable trap on Page Fault',
                    7: 'NXM: Child - enable trap on Non-Existent Memory',
                    8: 'MXM: Child - enable trap on Memory Access Violation',
                    9: 'ILL: Child - enable trap on Illegal Operation'
                }
            },
            'MTE1': {
                bits: 32,
                labels: {
                    11: 'PE', 12: 'PF', 13: 'PI', 14: 'PD', 15: 'PS',
                    16: 'PO', 17: 'PU', 18: 'PZ', 19: 'PM', 20: 'PK',
                    21: 'PX', 22: 'PN', 23: 'PC', 24: 'PL', 25: 'PW',
                    26: 'PG', 27: 'PV', 28: 'PT', 29: 'PR', 30: 'PA', 31: 'PY'
                },
                descriptions: {
                    11: 'PE: Mother process - enable trap on Protected Execute',
                    12: 'PF: Mother process - enable trap on Divide by Zero',
                    13: 'PI: Mother process - enable trap on Illegal Instruction',
                    14: 'PD: Mother process - enable trap on Illegal Data',
                    15: 'PS: Mother process - enable trap on Instruction Sequence Error',
                    16: 'PO: Mother process - enable trap on Illegal I/O',
                    17: 'PU: Mother process - enable Unassigned Trap',
                    18: 'PZ: Mother process - enable Unassigned Trap',
                    19: 'PM: Mother process - enable trap on Stack Overflow',
                    20: 'PK: Mother process - enable trap on Stack Underflow',
                    21: 'PX: Mother process - enable trap on FP Exception',
                    22: 'PN: Mother process - enable trap on FP Underflow',
                    23: 'PC: Mother process - enable trap on FP Overflow',
                    24: 'PL: Mother process - enable trap on Power Fail',
                    25: 'PW: Mother process - enable trap on Watchdog',
                    26: 'PG: Mother process - enable trap on Single Step',
                    27: 'PV: Mother process - enable trap on Breakpoint',
                    28: 'PT: Mother process - enable trap for Trace',
                    29: 'PR: Mother process - enable trap on Branch',
                    30: 'PA: Mother process - enable trap on Call',
                    31: 'PY: Mother process - enable trap on Return'
                }
            },
            'MTE2': {
                bits: 32,
                labels: {
                    0: 'XSE', 1: 'IIC', 2: 'IOS', 3: 'ISE', 4: 'PV',
                    5: 'THM', 6: 'PGF', 7: 'NXM', 8: 'MXM', 9: 'ILL'
                },
                descriptions: {
                    0: 'XSE: Mother - enable trap on External Sequence Error',
                    1: 'IIC: Mother - enable trap on Illegal Instruction Combination',
                    2: 'IOS: Mother - enable trap on I/O Status Error',
                    3: 'ISE: Mother - enable trap on Internal Sequence Error',
                    4: 'PV: Mother - enable trap on Parity/Protect Violation',
                    5: 'THM: Mother - enable trap on Too High Memory',
                    6: 'PGF: Mother - enable trap on Page Fault',
                    7: 'NXM: Mother - enable trap on Non-Existent Memory',
                    8: 'MXM: Mother - enable trap on Memory Access Violation',
                    9: 'ILL: Mother - enable trap on Illegal Operation'
                }
            },
            'TEMM1': {
                bits: 32,
                labels: {
                    11: 'PE', 12: 'PF', 13: 'PI', 14: 'PD', 15: 'PS',
                    16: 'PO', 17: 'PU', 18: 'PZ', 19: 'PM', 20: 'PK',
                    21: 'PX', 22: 'PN', 23: 'PC', 24: 'PL', 25: 'PW',
                    26: 'PG', 27: 'PV', 28: 'PT', 29: 'PR', 30: 'PA', 31: 'PY'
                },
                descriptions: {
                    11: 'PE: Trap Enable Mask - allow modifying PE bit',
                    12: 'PF: Trap Enable Mask - allow modifying PF bit',
                    13: 'PI: Trap Enable Mask - allow modifying PI bit',
                    14: 'PD: Trap Enable Mask - allow modifying PD bit',
                    15: 'PS: Trap Enable Mask - allow modifying PS bit',
                    16: 'PO: Trap Enable Mask - allow modifying PO bit',
                    17: 'PU: Trap Enable Mask - allow modifying PU bit',
                    18: 'PZ: Trap Enable Mask - allow modifying PZ bit',
                    19: 'PM: Trap Enable Mask - allow modifying PM bit',
                    20: 'PK: Trap Enable Mask - allow modifying PK bit',
                    21: 'PX: Trap Enable Mask - allow modifying PX bit',
                    22: 'PN: Trap Enable Mask - allow modifying PN bit',
                    23: 'PC: Trap Enable Mask - allow modifying PC bit',
                    24: 'PL: Trap Enable Mask - allow modifying PL bit',
                    25: 'PW: Trap Enable Mask - allow modifying PW bit',
                    26: 'PG: Trap Enable Mask - allow modifying PG bit',
                    27: 'PV: Trap Enable Mask - allow modifying PV bit',
                    28: 'PT: Trap Enable Mask - allow modifying PT bit',
                    29: 'PR: Trap Enable Mask - allow modifying PR bit',
                    30: 'PA: Trap Enable Mask - allow modifying PA bit',
                    31: 'PY: Trap Enable Mask - allow modifying PY bit'
                }
            },
            'TEMM2': {
                bits: 32,
                labels: {
                    0: 'XSE', 1: 'IIC', 2: 'IOS', 3: 'ISE', 4: 'PV',
                    5: 'THM', 6: 'PGF', 7: 'NXM', 8: 'MXM', 9: 'ILL'
                },
                descriptions: {
                    0: 'XSE: Trap Enable Mask - allow modifying XSE bit',
                    1: 'IIC: Trap Enable Mask - allow modifying IIC bit',
                    2: 'IOS: Trap Enable Mask - allow modifying IOS bit',
                    3: 'ISE: Trap Enable Mask - allow modifying ISE bit',
                    4: 'PV: Trap Enable Mask - allow modifying PV bit',
                    5: 'THM: Trap Enable Mask - allow modifying THM bit',
                    6: 'PGF: Trap Enable Mask - allow modifying PGF bit',
                    7: 'NXM: Trap Enable Mask - allow modifying NXM bit',
                    8: 'MXM: Trap Enable Mask - allow modifying MXM bit',
                    9: 'ILL: Trap Enable Mask - allow modifying ILL bit'
                }
            }
        };

        return BIT_DEFINITIONS[regName] || null;
    }

    getRegisterDescription(regName) {
        const REGISTER_DESCRIPTIONS = {
            // CPU Registers
            'PC': 'Program Counter',
            'I1': 'Integer Register 1',
            'I2': 'Integer Register 2',
            'I3': 'Integer Register 3',
            'I4': 'Integer Register 4',
            'A1': 'Float Accumulator 1',
            'A2': 'Float Accumulator 2',
            'A3': 'Float Accumulator 3',
            'A4': 'Float Accumulator 4',
            'E1': 'Float Extension 1',
            'E2': 'Float Extension 2',
            'E3': 'Float Extension 3',
            'E4': 'Float Extension 4',
            'L': 'Link Register',
            'B': 'Base Register',
            'R': 'Record Register',
            'TOS': 'Top of Stack',
            'LL': 'Lower Limit',
            'HL': 'Higher Limit',
            'THA': 'Trap Handler Address',
            'ST1': 'Status Register Upper',
            'ST2': 'Status Register Lower',
            'FLAGS': 'CPU Flags',
            // MMU Registers
            'PSTP': 'Physical Segment Table Pointer',
            'DITBASE': 'Domain Information Table Base',
            'CED': 'Current Executing Domain',
            'CAD': 'Current Alternative Domain',
            'PS': 'Process Segment',
            // Control Registers
            'OTE1': 'Trap Enable Upper',
            'OTE2': 'Trap Enable Lower',
            'CTE1': 'Calculated Trap Enable Upper',
            'CTE2': 'Calculated Trap Enable Lower',
            'MTE1': 'Monitor Trap Enable Upper',
            'MTE2': 'Monitor Trap Enable Lower',
            'TEMM1': 'Trap Emulation Mask Upper',
            'TEMM2': 'Trap Emulation Mask Lower'
        };
        return REGISTER_DESCRIPTIONS[regName] || null;
    }

    editRegister(element) {
        const regName = element.dataset.reg;
        const currentValue = parseInt(element.dataset.value);

        // Check if this register needs bit editor
        if (this.getBitDefinition(regName)) {
            this.openBitEditor(regName, currentValue);
        } else {
            // Use modal for simple numeric editor
            this.openRegisterEditor(regName, currentValue);
        }
    }

    openRegisterEditor(regName, currentValue) {
        const modal = document.getElementById('regEditorModal');
        const regNameSpan = document.getElementById('regEditorRegName');
        const currentValueSpan = document.getElementById('regEditorCurrentValue');
        const input = document.getElementById('regEditorInput');
        const cancelBtn = document.getElementById('regEditorCancelBtn');
        const applyBtn = document.getElementById('regEditorApplyBtn');

        // Set header with description
        const description = this.getRegisterDescription(regName);
        regNameSpan.textContent = description ? `${regName} (${description})` : regName;
        currentValueSpan.textContent = '0x' + currentValue.toString(16).padStart(8, '0').toUpperCase();
        input.value = '0x' + currentValue.toString(16).padStart(8, '0').toUpperCase();

        // Show modal
        modal.classList.remove('hidden');
        input.focus();
        input.select();

        // Handle Enter key in input
        const enterHandler = (e) => {
            if (e.key === 'Enter') {
                applyBtn.click();
            } else if (e.key === 'Escape') {
                cancelBtn.click();
            }
        };
        input.addEventListener('keydown', enterHandler);

        // Cancel handler
        const cancelHandler = () => {
            modal.classList.add('hidden');
            input.removeEventListener('keydown', enterHandler);
            cancelBtn.removeEventListener('click', cancelHandler);
            applyBtn.removeEventListener('click', applyHandler);
        };

        // Apply handler
        const applyHandler = () => {
            const newValue = input.value.trim();
            if (!newValue) {
                alert('Please enter a value');
                return;
            }

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

                // Close modal
                cancelHandler();
            } catch (error) {
                console.error('Error setting register:', error);
                alert('Error setting register: ' + error.message);
            }
        };

        cancelBtn.addEventListener('click', cancelHandler);
        applyBtn.addEventListener('click', applyHandler);
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}

// Console Manager for interactive command-line interface
class ConsoleManager {
    constructor(dbg) {
        this.dbg = dbg;
        this.history = [];
        this.historyIndex = -1;
        this.commands = [];
        this.modal = document.getElementById('consoleModal');
        this.output = document.getElementById('console-output');
        this.input = document.getElementById('consoleInput');
        this.clearBtn = document.getElementById('consoleClearBtn');
        this.closeBtn = document.getElementById('consoleCancelBtn');
        this.setupEventHandlers();
        this.loadCommands();
    }

    setupEventHandlers() {
        // Console button opens modal
        document.getElementById('consoleBtn').onclick = () => {
            this.show();
        };

        // Close button
        this.closeBtn.onclick = () => {
            this.hide();
        };

        // Clear button
        this.clearBtn.onclick = () => {
            this.clear();
        };

        // Input field handlers
        this.input.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                e.preventDefault();
                this.executeCommand();
            } else if (e.key === 'ArrowUp') {
                e.preventDefault();
                this.navigateHistory(-1);
            } else if (e.key === 'ArrowDown') {
                e.preventDefault();
                this.navigateHistory(1);
            } else if (e.key === 'Tab') {
                e.preventDefault();
                this.autocomplete();
            }
        });

        // Click outside modal to close
        this.modal.addEventListener('click', (e) => {
            if (e.target === this.modal) {
                this.hide();
            }
        });
    }

    async loadCommands() {
        if (!this.dbg.module) return;
        try {
            const json = this.dbg.module.ccall('nd500_cmd_list_js', 'string', [], []);
            this.commands = JSON.parse(json);
            console.log('Loaded commands:', this.commands);
        } catch (error) {
            console.error('Error loading commands:', error);
            this.commands = [];
        }
    }

    show() {
        this.modal.classList.remove('hidden');
        this.input.focus();
        // Scroll to bottom
        this.output.scrollTop = this.output.scrollHeight;
    }

    hide() {
        this.modal.classList.add('hidden');
    }

    clear() {
        this.output.innerHTML = '';
    }

    addLine(text, className = 'output') {
        const line = document.createElement('div');
        line.className = `console-line ${className}`;
        line.textContent = text;
        this.output.appendChild(line);
        // Auto-scroll to bottom
        this.output.scrollTop = this.output.scrollHeight;
    }

    executeCommand() {
        const cmdline = this.input.value.trim();
        if (!cmdline) return;

        // Add to history
        if (this.history.length === 0 || this.history[this.history.length - 1] !== cmdline) {
            this.history.push(cmdline);
        }
        this.historyIndex = this.history.length;

        // Display command
        this.addLine('> ' + cmdline, 'command');

        // Clear input
        this.input.value = '';

        // Execute via WASM
        try {
            const output = this.dbg.module.ccall('nd500_cmd_exec_js', 'string', ['string'], [cmdline]);
            if (output && output.trim()) {
                // Split output into lines and add each
                const lines = output.split('\n');
                lines.forEach(line => {
                    if (line.trim()) {
                        // Check if line contains error indicators
                        const isError = line.toLowerCase().includes('error') ||
                                      line.toLowerCase().includes('invalid') ||
                                      line.toLowerCase().includes('failed') ||
                                      line.toLowerCase().includes('unknown');
                        this.addLine(line, isError ? 'error' : 'output');
                    }
                });
            }

            // Update UI after command execution
            this.dbg.updateUI();
        } catch (error) {
            console.error('Error executing command:', error);
            this.addLine('Error: ' + error.message, 'error');
        }
    }

    navigateHistory(direction) {
        if (this.history.length === 0) return;

        this.historyIndex += direction;

        // Clamp to valid range
        if (this.historyIndex < 0) {
            this.historyIndex = 0;
        } else if (this.historyIndex >= this.history.length) {
            this.historyIndex = this.history.length;
            this.input.value = '';
            return;
        }

        this.input.value = this.history[this.historyIndex];
    }

    autocomplete() {
        const text = this.input.value;
        const words = text.split(/\s+/);

        if (words.length === 0) return;

        if (words.length === 1) {
            // Autocomplete command
            const partial = words[0].toLowerCase();
            const matches = this.commands.filter(cmd => cmd.toLowerCase().startsWith(partial));

            if (matches.length === 1) {
                // Single match - complete it
                this.input.value = matches[0] + ' ';
            } else if (matches.length > 1) {
                // Multiple matches - show them
                this.addLine('> ' + text, 'command');
                this.addLine('Possible commands: ' + matches.join(', '), 'output');
            }
        } else {
            // Autocomplete subcommand
            const command = words[0];
            const partial = words[words.length - 1].toLowerCase();

            try {
                const json = this.dbg.module.ccall('nd500_cmd_subcommands_js', 'string', ['string'], [command]);
                const subcommands = JSON.parse(json);

                if (subcommands.length === 0) return;

                const matches = subcommands.filter(sub => sub.toLowerCase().startsWith(partial));

                if (matches.length === 1) {
                    // Single match - complete it
                    words[words.length - 1] = matches[0];
                    this.input.value = words.join(' ') + ' ';
                } else if (matches.length > 1) {
                    // Multiple matches - show them
                    this.addLine('> ' + text, 'command');
                    this.addLine('Possible subcommands: ' + matches.join(', '), 'output');
                }
            } catch (error) {
                console.error('Error getting subcommands:', error);
            }
        }
    }
}

// Initialize when WASM loads (support factory or legacy)
let nd500Debugger;
let consoleManager;

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

        // Create console manager
        console.log('Creating ConsoleManager...');
        consoleManager = new ConsoleManager(nd500Debugger);
        window.consoleManager = consoleManager;
        console.log('ConsoleManager initialized successfully');

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
