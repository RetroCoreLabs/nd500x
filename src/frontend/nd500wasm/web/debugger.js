class ND500Debugger {
    constructor() {
        this.module = null;
        this.currentPC = 0;
        this.breakpoints = [];
        this.isRunning = false;
        this.runInterval = null;
        this.VERSION = '20251021e'; // Update this with each change
        this.memoryMapDomainFilter = 'all'; // Default to showing all domains

        // Source-level debugging state
        this.sourceFiles = new Map(); // filename -> content
        this.currentSourceFile = null;
        this.activeTab = 'disasm'; // 'disasm', 'asm', or 'c'
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

        // Source Files button
        document.getElementById('sourceBtn').onclick = () => this.openSourceModal();

        // Code view tabs
        document.querySelectorAll('.code-tab').forEach(tab => {
            tab.onclick = () => this.switchCodeTab(tab.dataset.tab);
        });

        // Assembly file dropdown
        document.getElementById('asm-file-dropdown').onchange = (e) => {
            this.renderAsmSourceView();
        };

        // C file dropdown
        document.getElementById('c-file-dropdown').onchange = (e) => {
            this.renderCSourceView();
        };

        // Source file selection events
        document.getElementById('sourceFileInput').onchange = (e) => {
            this.updateFileName('sourceFileName', e.target.files[0]);
        };
        document.getElementById('mapFileInput').onchange = (e) => {
            this.updateFileName('mapFileName', e.target.files[0]);
        };
        document.getElementById('zipFileInput').onchange = (e) => {
            this.updateFileName('zipFileName', e.target.files[0]);
        };
    }

    openLoadModal() {
        const modal = document.getElementById('loadModal');
        if (!modal) {
            console.error('Load modal not found');
            return;
        }

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
        const mmuProgramValue = document.getElementById('loadModalMmuProgramValue');
        const mmuDataValue = document.getElementById('loadModalMmuDataValue');
        const mmuStatusHint = document.getElementById('loadModalMmuHint');
        const mmuProgramToggleBtn = document.getElementById('loadModalMmuProgramToggle');
        const mmuDataToggleBtn = document.getElementById('loadModalMmuDataToggle');

        // Verify critical elements exist
        if (!modeSelect || !domainSelect || !mmuStatusBanner || !mmuProgramValue || !mmuDataValue) {
            console.error('Load modal elements missing');
            return;
        }

        const updateMmuStatus = () => {
            if (this.module && modeSelect) {
                const programEnabled = this.module.ccall('nd500_dbg_mmu_is_program_enabled_js', 'number', [], []);
                const dataEnabled = this.module.ccall('nd500_dbg_mmu_is_data_enabled_js', 'number', [], []);
                const mode = modeSelect.value;

                // Get Mode and Domain rows
                const modeRow = modeSelect.closest('.row');
                const domainRow = domainSelect.closest('.row');
                const psegAddrInput = document.getElementById('psegAddr');
                const dsegAddrInput = document.getElementById('dsegAddr');

                // Update Program MMU status
                if (mmuProgramValue) {
                    mmuProgramValue.textContent = programEnabled ? 'ON' : 'OFF';
                    mmuProgramValue.style.color = programEnabled ? '#4CAF50' : '#f44336';
                }
                if (mmuProgramToggleBtn) {
                    mmuProgramToggleBtn.textContent = programEnabled ? 'PMOF' : 'PMON';
                }

                // Update Data MMU status
                if (mmuDataValue) {
                    mmuDataValue.textContent = dataEnabled ? 'ON' : 'OFF';
                    mmuDataValue.style.color = dataEnabled ? '#4CAF50' : '#f44336';
                }
                if (mmuDataToggleBtn) {
                    mmuDataToggleBtn.textContent = dataEnabled ? 'DMOF' : 'DMON';
                }

                const mmuEnabled = programEnabled || dataEnabled;

                if (mmuEnabled) {
                    // MMU Enabled: Show Mode and Domain, use virtual addresses
                    mmuStatusBanner.style.backgroundColor = '#e8f5e9';

                    // Show Mode and Domain fields
                    if (modeRow) modeRow.style.display = '';
                    if (domainRow) domainRow.style.display = '';

                    // Get selected domain
                    const domain = parseInt(domainSelect.value, 10);

                    // mmusetup only configures domains 0, 1, 2
                    const configuredDomains = [0, 1, 2];
                    const isDomainConfigured = configuredDomains.includes(domain);

                    // Update hint and placeholders based on domain
                    if (domain === 0) {
                        mmuStatusHint.textContent = 'Domain 0 (kernel): Code 0x08000000-0x0FFFFFFF, Data 0x00000000-0x07FFFFFF';
                        mmuStatusHint.style.color = '';  // Reset to default color
                        if (psegAddrInput) psegAddrInput.placeholder = 'Default: 0x08000000 (kernel code)';
                        if (dsegAddrInput) dsegAddrInput.placeholder = 'Default: 0x00000000 (kernel data)';
                    } else if (isDomainConfigured) {
                        // Domain 1-2: Segment base = domain << 27
                        const codeBase = (26 + domain) << 27;  // Segment 26 + domain
                        const dataBase = (30 + domain) << 27;  // Segment 30 + domain
                        const codeEnd = codeBase + 0x07FFFFFF;
                        const dataEnd = dataBase + 0x07FFFFFF;
                        mmuStatusHint.textContent = `Domain ${domain}: Code 0x${codeBase.toString(16).toUpperCase()}-0x${codeEnd.toString(16).toUpperCase()}, Data 0x${dataBase.toString(16).toUpperCase()}-0x${dataEnd.toString(16).toUpperCase()}`;
                        mmuStatusHint.style.color = '';  // Reset to default color
                        if (psegAddrInput) psegAddrInput.placeholder = `Default: 0x${codeBase.toString(16).toUpperCase()} (domain ${domain} code)`;
                        if (dsegAddrInput) dsegAddrInput.placeholder = `Default: 0x${dataBase.toString(16).toUpperCase()} (domain ${domain} data)`;
                    } else {
                        // Domain 3+: Not configured by mmusetup
                        mmuStatusHint.textContent = `Domain ${domain}: No virtual address space configured (run 'mmusetup' only configures domains 0-2)`;
                        mmuStatusHint.style.color = '#f44336';
                        if (psegAddrInput) psegAddrInput.placeholder = 'Domain not configured - no valid addresses';
                        if (dsegAddrInput) dsegAddrInput.placeholder = 'Domain not configured - no valid addresses';
                    }
                } else {
                    // MMU Disabled: Hide Mode and Domain, use physical addresses
                    mmuStatusHint.textContent = 'Physical memory: 0x00000000-0x00FFFFFF (16MB) - Direct addressing, no domains';
                    mmuStatusBanner.style.backgroundColor = '#ffebee';

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

        // Update addresses when domain changes
        domainSelect.onchange = () => {
            updateMmuStatus();
        };

        // Initial MMU status update
        updateMmuStatus();

        // Program MMU toggle button handler
        if (mmuProgramToggleBtn) {
            mmuProgramToggleBtn.onclick = async () => {
                if (this.module) {
                    const isEnabled = this.module.ccall('nd500_dbg_mmu_is_program_enabled_js', 'number', [], []);

                    if (isEnabled) {
                        this.module.ccall('nd500_dbg_mmu_disable_program_js', 'void', [], []);
                        this.updateStatus('Program MMU disabled (PMOF)');
                    } else {
                        this.module.ccall('nd500_dbg_mmu_enable_program_js', 'void', [], []);
                        this.updateStatus('Program MMU enabled (PMON)');
                    }

                    updateMmuStatus();
                    this.updateUI();
                }
            };
        }

        // Data MMU toggle button handler
        if (mmuDataToggleBtn) {
            mmuDataToggleBtn.onclick = async () => {
                if (this.module) {
                    const isEnabled = this.module.ccall('nd500_dbg_mmu_is_data_enabled_js', 'number', [], []);

                    if (isEnabled) {
                        this.module.ccall('nd500_dbg_mmu_disable_data_js', 'void', [], []);
                        this.updateStatus('Data MMU disabled (DMOF)');
                    } else {
                        this.module.ccall('nd500_dbg_mmu_enable_data_js', 'void', [], []);
                        this.updateStatus('Data MMU enabled (DMON)');
                    }

                    updateMmuStatus();
                    this.updateUI();
                }
            };
        }

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
        const memoryMapRefreshBtn = document.getElementById('memoryMapRefreshBtn');

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
                    'pcb': 'pcbTabContent',
                    'memmap': 'memmapTabContent'
                };

                const content = document.getElementById(contentMap[targetTab]);
                if (content) {
                    content.classList.add('active');

                    // Load data for newly activated tab
                    if (targetTab === 'pst') {
                        this.updatePstTable();
                    } else if (targetTab === 'pcb') {
                        this.updatePcbDomainList();
                    } else if (targetTab === 'memmap') {
                        this.updateMemoryMap();
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

        // Toggle Program MMU (PMON/PMOF)
        const programToggleBtn = document.getElementById('mmuProgramToggleBtn');
        if (programToggleBtn) {
            programToggleBtn.onclick = () => {
                try {
                    const isEnabled = this.module.ccall('nd500_dbg_mmu_is_program_enabled_js', 'number', [], []);

                    if (isEnabled) {
                        this.module.ccall('nd500_dbg_mmu_disable_program_js', 'void', [], []);
                        this.updateStatus('Program MMU disabled (PMOF)');
                    } else {
                        this.module.ccall('nd500_dbg_mmu_enable_program_js', 'void', [], []);
                        this.updateStatus('Program MMU enabled (PMON)');
                    }

                    this.updateMmuModal();
                    this.updateMmuPanel();
                    this.updateUI();
                } catch (error) {
                    console.error('Error toggling Program MMU:', error);
                    this.updateStatus('Error toggling Program MMU');
                }
            };
        }

        // Toggle Data MMU (DMON/DMOF)
        const dataToggleBtn = document.getElementById('mmuDataToggleBtn');
        if (dataToggleBtn) {
            dataToggleBtn.onclick = () => {
                try {
                    const isEnabled = this.module.ccall('nd500_dbg_mmu_is_data_enabled_js', 'number', [], []);

                    if (isEnabled) {
                        this.module.ccall('nd500_dbg_mmu_disable_data_js', 'void', [], []);
                        this.updateStatus('Data MMU disabled (DMOF)');
                    } else {
                        this.module.ccall('nd500_dbg_mmu_enable_data_js', 'void', [], []);
                        this.updateStatus('Data MMU enabled (DMON)');
                    }

                    this.updateMmuModal();
                    this.updateMmuPanel();
                    this.updateUI();
                } catch (error) {
                    console.error('Error toggling Data MMU:', error);
                    this.updateStatus('Error toggling Data MMU');
                }
            };
        }

        // Toggle MMU on/off (legacy - kept for compatibility)
        if (toggleBtn) {
            toggleBtn.onclick = () => {
                try {
                const output = this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], ['mmu']);
                const isEnabled = output.includes('enabled');

                // Toggle state
                const newState = isEnabled ? 'off' : 'on';
                this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], [`mmu ${newState}`]);

                // Adjust PC based on new MMU state
                if (newState === 'on') {
                    // MMU enabled - set PC to kernel virtual address
                    this.module.ccall('nd500_dbg_set_pc_js', 'number', ['number'], [0x08000000]);
                } else {
                    // MMU disabled - reset PC to physical address
                    this.module.ccall('nd500_dbg_set_pc_js', 'number', ['number'], [0x00000000]);
                }

                // Update display
                this.updateMmuModal();
                this.updateMmuPanel();
                this.updateUI();  // Refresh disassembly after MMU state change
                this.updateStatus(`MMU ${newState === 'on' ? 'enabled' : 'disabled'}`);
                } catch (error) {
                    console.error('Error toggling MMU:', error);
                    this.updateStatus('Error toggling MMU');
                }
            };
        }

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

        // Memory Map refresh button
        memoryMapRefreshBtn.onclick = () => {
            this.updateMemoryMap();
            this.updateStatus('Memory map refreshed');
        };

        // Setup Memory Map domain filter
        const memoryMapDomainFilter = document.getElementById('memoryMapDomainFilter');
        if (memoryMapDomainFilter) {
            memoryMapDomainFilter.onchange = (event) => {
                this.memoryMapDomainFilter = memoryMapDomainFilter.value;
                this.updateMemoryMap();
            };
        }

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

        const pcbSegmentSearchInput = document.getElementById('pcbSegmentSearchInput');
        pcbSegmentSearchInput.oninput = () => {
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

            // Check separate Program and Data MMU states
            const programEnabled = this.module.ccall('nd500_dbg_mmu_is_program_enabled_js', 'number', [], []);
            const dataEnabled = this.module.ccall('nd500_dbg_mmu_is_data_enabled_js', 'number', [], []);

            // Update Program MMU state display
            const programStateElem = document.getElementById('mmuProgramState');
            const programToggleBtn = document.getElementById('mmuProgramToggleBtn');

            if (programStateElem && programToggleBtn) {
                if (programEnabled) {
                    programStateElem.textContent = 'enabled';
                    programStateElem.className = 'mmu-value enabled';
                    programToggleBtn.textContent = 'Disable (PMOF)';
                    programToggleBtn.className = 'mmu-toggle-btn disable';
                } else {
                    programStateElem.textContent = 'disabled';
                    programStateElem.className = 'mmu-value disabled';
                    programToggleBtn.textContent = 'Enable (PMON)';
                    programToggleBtn.className = 'mmu-toggle-btn';
                }
            }

            // Update Data MMU state display
            const dataStateElem = document.getElementById('mmuDataState');
            const dataToggleBtn = document.getElementById('mmuDataToggleBtn');

            if (dataStateElem && dataToggleBtn) {
                if (dataEnabled) {
                    dataStateElem.textContent = 'enabled';
                    dataStateElem.className = 'mmu-value enabled';
                    dataToggleBtn.textContent = 'Disable (DMOF)';
                    dataToggleBtn.className = 'mmu-toggle-btn disable';
                } else {
                    dataStateElem.textContent = 'disabled';
                    dataStateElem.className = 'mmu-value disabled';
                    dataToggleBtn.textContent = 'Enable (DMON)';
                    dataToggleBtn.className = 'mmu-toggle-btn';
                }
            }

            // Parse PST count from output
            const pstMatch = output.match(/PST: (\d+) configured entries \(of (\d+) max\)/);
            const mmuPstCount = document.getElementById('mmuPstCount');
            if (pstMatch && mmuPstCount) {
                mmuPstCount.textContent = `${pstMatch[1]} configured (of ${pstMatch[2]} max)`;
            }

            // Parse PCB count from output
            const pcbMatch = output.match(/PCB: (\d+) domains with (\d+) segments/);
            const mmuPcbCount = document.getElementById('mmuPcbCount');
            if (pcbMatch && mmuPcbCount) {
                mmuPcbCount.textContent = `${pcbMatch[1]} domains with ${pcbMatch[2]} segments`;
            }

            // Get registers from JSON
            const json = this.module.ccall('nd500_dbg_regs_json', 'string', [], []);
            const regs = JSON.parse(json);

            // Update MMU registers
            const regPSTP = document.getElementById('regPSTP');
            if (regPSTP && regs.PSTP !== undefined) {
                regPSTP.textContent = '0x' + regs.PSTP.toString(16).padStart(8,'0').toUpperCase();
            }
            const regDITBASE = document.getElementById('regDITBASE');
            if (regDITBASE && regs.DITBASE !== undefined) {
                regDITBASE.textContent = '0x' + regs.DITBASE.toString(16).padStart(8,'0').toUpperCase();
            }
            const regCED = document.getElementById('regCED');
            if (regCED && regs.CED !== undefined) {
                regCED.textContent = '0x' + regs.CED.toString(16).padStart(8,'0').toUpperCase();
            }
            const regCAD = document.getElementById('regCAD');
            if (regCAD && regs.CAD !== undefined) {
                regCAD.textContent = '0x' + regs.CAD.toString(16).padStart(8,'0').toUpperCase();
            }
            const regPS = document.getElementById('regPS');
            if (regPS && regs.PS !== undefined) {
                regPS.textContent = '0x' + regs.PS.toString(16).padStart(8,'0').toUpperCase();
            }
        } catch (error) {
            console.error('Error updating MMU modal:', error);
        }
    }

    updateMmuPanel() {
        if (!this.module) return;

        try {
            // Get separate Program and Data MMU status
            const programEnabled = this.module.ccall('nd500_dbg_mmu_is_program_enabled_js', 'number', [], []);
            const dataEnabled = this.module.ccall('nd500_dbg_mmu_is_data_enabled_js', 'number', [], []);

            // Get MMU status output for counts
            const output = this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], ['showmmu']);

            // Parse counts
            const pstMatch = output.match(/PST: (\d+) configured entries/);
            const pcbMatch = output.match(/PCB: (\d+) domains with (\d+) segments/);

            const pstCount = pstMatch ? pstMatch[1] : '0';
            const pcbDomains = pcbMatch ? pcbMatch[1] : '0';
            const pcbSegments = pcbMatch ? pcbMatch[2] : '0';

            // Create inline status display with separate I&D
            const programClass = programEnabled ? 'enabled' : 'disabled';
            const programText = programEnabled ? 'ON' : 'OFF';
            const dataClass = dataEnabled ? 'enabled' : 'disabled';
            const dataText = dataEnabled ? 'ON' : 'OFF';

            const html = `
                <div class="mmu-status-inline">
                    <div class="mmu-status-inline-row">
                        <span class="mmu-status-inline-label">Program MMU:</span>
                        <span class="mmu-status-inline-value ${programClass}">${programText}</span>
                    </div>
                    <div class="mmu-status-inline-row">
                        <span class="mmu-status-inline-label">Data MMU:</span>
                        <span class="mmu-status-inline-value ${dataClass}">${dataText}</span>
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

            // Apply search filters
            const domainSearchText = document.getElementById('pcbSearchInput').value.toLowerCase().trim();
            const segmentSearchText = document.getElementById('pcbSegmentSearchInput').value.toLowerCase().trim();
            let filtered = domains;

            // Filter by domain
            if (domainSearchText) {
                filtered = filtered.filter(d => d.domain.toString().includes(domainSearchText));
            }

            // Filter by segment within each domain
            if (segmentSearchText) {
                filtered = filtered.map(d => {
                    return {
                        ...d,
                        segments: d.segments.filter(s => s.segment.toString().includes(segmentSearchText))
                    };
                }).filter(d => d.segments.length > 0); // Only show domains that have matching segments
            }

            // Update stats
            const totalSegments = filtered.reduce((sum, d) => sum + d.segments.length, 0);
            const totalDomains = domains.length;
            const totalAllSegments = domains.reduce((sum, d) => sum + d.segments.length, 0);

            let statsText = `${filtered.length} domains shown with ${totalSegments} segments`;
            if (filtered.length !== totalDomains || totalSegments !== totalAllSegments) {
                statsText += ` (of ${totalDomains} total domains with ${totalAllSegments} segments)`;
            }
            document.getElementById('pcbTableStats').textContent = statsText;

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
                <div class="pcb-segments" data-domain="${domain.domain}">`;

        // Render each segment with detailed bit layout
        domain.segments.forEach(seg => {
            // Fetch detailed capability data from new JSON API
            let capData = null;
            try {
                const jsonStr = this.module.ccall('nd500_dbg_pcb_segment_json_js', 'string', ['number', 'number'], [domain.domain, seg.segment]);
                capData = JSON.parse(jsonStr);
            } catch (error) {
                console.error(`Error fetching capability data for domain ${domain.domain}, segment ${seg.segment}:`, error);
            }

            if (capData && capData.program && capData.data) {
                html += this.renderSegmentDetailed(capData);
            } else {
                // Fallback to simple display if JSON API fails
                html += `<div class="segment-card">
                    <div class="segment-header">Segment ${seg.segment} - Error loading details</div>
                </div>`;
            }
        });

        html += `
                </div>
            </div>`;

        return html;
    }

    renderSegmentDetailed(capData) {
        const domain = capData.domain;
        const segment = capData.segment;
        const prog = capData.program;
        const data = capData.data;

        // Get segment name hint
        const segmentName = this.getSegmentNameHint(segment);

        let html = `
            <div class="segment-card">
                <div class="segment-header">
                    <span class="segment-number">Segment ${segment}</span>
                    ${segmentName ? `<span class="segment-hint">${segmentName}</span>` : ''}
                    <button class="pcb-action-btn-small" onclick="event.stopPropagation(); nd500Debugger.openPcbEditModal(${domain}, ${segment}, ${prog.raw}, ${data.raw});">✎ Edit</button>
                </div>

                <!-- Program Capability -->
                <div class="capability-section prog-cap-${prog.type.toLowerCase()}">
                    <div class="capability-title">Program Capability: ${prog.type}</div>
                    <div class="capability-raw">Raw: 0x${prog.raw.toString(16).padStart(4,'0').toUpperCase()}</div>

                    <div class="bit-layout">`;

        if (prog.bit15 === 0) {
            // DIRECT layout
            html += `
                        <span class="bit-field" title="bit 15: Direct=0, Indirect=1">0</span>
                        <span class="bit-field" title="bits 14-13: unused">${prog.unused_bits.toString(2).padStart(2,'0')}</span>
                        <span class="bit-field wide" title="bits 12-0: Physical Segment Number">0x${prog.psn.toString(16).padStart(4,'0').toUpperCase()}</span>
                    </div>
                    <div class="bit-labels">
                        <span>Dir</span>
                        <span>Unused</span>
                        <span>PSN ${prog.psn} → PST[${prog.psn}]</span>
                    </div>`;
        } else {
            // INDIRECT layout
            html += `
                        <span class="bit-field" title="bit 15: Direct=0, Indirect=1">1</span>
                        <span class="bit-field" title="bit 14: Other Machine">${prog.omc_bit}</span>
                        <span class="bit-field" title="bit 13: unused">${prog.unused_bit13}</span>
                        <span class="bit-field" title="bits 12-5: Domain">0x${prog.domain.toString(16).padStart(2,'0').toUpperCase()}</span>
                        <span class="bit-field" title="bits 4-0: Segment">0x${prog.segment.toString(16).padStart(2,'0').toUpperCase()}</span>
                    </div>
                    <div class="bit-labels">
                        <span>Ind</span>
                        <span>OMC${prog.omc_bit ? '✅' : '❌'}</span>
                        <span>Unu</span>
                        <span>Domain ${prog.domain}</span>
                        <span>Segment ${prog.segment}</span>
                    </div>
                    <div class="indirect-pointer">→ Points to Domain ${prog.domain}, Segment ${prog.segment}</div>`;
        }

        html += `
                </div>

                <!-- Data Capability -->
                <div class="capability-section data-cap">
                    <div class="capability-title">Data Capability</div>
                    <div class="capability-raw">Raw: 0x${data.raw.toString(16).padStart(4,'0').toUpperCase()}</div>

                    <div class="bit-layout">
                        <span class="bit-field" title="bit 15: Write Permitted">${data.wrp_bit}</span>
                        <span class="bit-field" title="bit 14: Parameter Access (User)">${data.pac_bit}</span>
                        <span class="bit-field" title="bit 13: Shared Segment">${data.shs_bit}</span>
                        <span class="bit-field wide" title="bits 12-0: Physical Segment Number">0x${data.psn.toString(16).padStart(4,'0').toUpperCase()}</span>
                    </div>
                    <div class="bit-labels">
                        <span>WRP${data.wrp_bit ? '✅' : '❌'}</span>
                        <span>PAC${data.pac_bit ? '✅' : '❌'}</span>
                        <span>SHS${data.shs_bit ? '✅' : '❌'}</span>
                        <span>PSN ${data.psn} → PST[${data.psn}]</span>
                    </div>
                    ${data.permission !== 'NONE' ? `
                    <div class="permission-badge ${data.permission.toLowerCase().replace(/_/g, '-')}">
                        ${data.permission}: ${data.permission_desc}
                    </div>` : ''}
                </div>
            </div>`;

        return html;
    }

    getSegmentNameHint(segment) {
        const hints = {
            0: 'Kernel Data',
            1: 'Kernel Text',
            2: 'Physical Memory',
            3: 'System Tables',
            4: 'User Page Tables',
            5: 'Shadow Page Tables',
            6: 'Shared Segment',
            7: 'System Tables (NC)',
            8: 'Context Block Table',
            26: 'User Text',
            27: 'PST',
            28: 'Process Segment (PCB)',
            29: 'Kernel Stack',
            30: 'User Data',
            31: 'User Stack / Other Machine'
        };
        return hints[segment] || null;
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

    updateMemoryMap() {
        if (!this.module) return;

        try {
            // Get memory map JSON from WASM (filtered by domain if selected)
            let jsonStr;
            if (this.memoryMapDomainFilter === 'all') {
                jsonStr = this.module.ccall('nd500_dbg_memory_map_json_js', 'string', [], []);
            } else {
                const domain = parseInt(this.memoryMapDomainFilter, 10);
                jsonStr = this.module.ccall('nd500_dbg_memory_map_for_domain_json_js', 'string', ['number'], [domain]);
            }

            const data = JSON.parse(jsonStr);

            if (data.error) {
                throw new Error(data.error);
            }

            // Filter blocks by accessibility if domain filter is active
            let visibleBlocks = data.blocks;
            if (this.memoryMapDomainFilter !== 'all') {
                visibleBlocks = data.blocks.filter(block => block.accessible_from_filter === true);
            }

            // Calculate memory usage (only count visible blocks)
            const totalMem = data.total_memory;
            const usedMem = visibleBlocks.reduce((sum, block) => sum + block.size, 0);
            const usedPercent = totalMem > 0 ? (usedMem / totalMem * 100).toFixed(1) : 0;
            const freeMem = totalMem - usedMem;

            // Update stats
            const statEl = document.getElementById('memoryUsedStat');
            if (this.memoryMapDomainFilter === 'all') {
                statEl.textContent = `${this.formatSize(usedMem)} used / ${this.formatSize(totalMem)} total (${usedPercent}% used, ${this.formatSize(freeMem)} free)`;
            } else {
                statEl.textContent = `Domain ${this.memoryMapDomainFilter}: ${this.formatSize(usedMem)} accessible / ${this.formatSize(totalMem)} total (${usedPercent}% accessible)`;
            }

            // Update usage bar
            const usedBar = document.getElementById('memoryUsedBar');
            usedBar.style.width = `${usedPercent}%`;

            // Render memory blocks (only visible ones after filtering)
            this.renderMemoryBlocks(visibleBlocks, totalMem);

        } catch (error) {
            console.error('Error updating memory map:', error);
            document.getElementById('memoryUsedStat').textContent = 'Error loading memory map';
            document.getElementById('memoryMapBlocks').innerHTML = '<div class="memory-map-error">Error loading memory map</div>';
        }
    }

    renderMemoryBlocks(blocks, totalMem) {
        const container = document.getElementById('memoryMapBlocks');
        const infoPanel = document.getElementById('memoryMapInfo');

        if (blocks.length === 0) {
            container.innerHTML = '<div class="memory-map-empty">No memory mapped. Use \'mmusetup\' to create a demo configuration.</div>';
            return;
        }

        // Sort blocks by physical address (should already be sorted from backend)
        blocks.sort((a, b) => a.phys_start - b.phys_start);

        // Use absolute positioning so blocks stay in their physical address locations
        let html = '';

        blocks.forEach((block, idx) => {
            // Calculate position and width based on absolute physical addresses
            const leftPercent = (block.phys_start / totalMem * 100);
            const widthPercent = (block.size / totalMem * 100);
            const color = this.getBlockColor(block.domain, block.segment);
            const title = this.getBlockTitle(block);

            html += `<div class="memory-block mapped"
                style="position: absolute; left: ${leftPercent}%; width: ${widthPercent}%; background-color: ${color};"
                data-block-index="${idx}"
                title="${title}">
            </div>`;
        });

        container.innerHTML = html;

        // Add hover handlers
        const blockEls = container.querySelectorAll('.memory-block.mapped');
        blockEls.forEach(blockEl => {
            const idx = parseInt(blockEl.dataset.blockIndex);
            const block = blocks[idx];

            blockEl.onmouseenter = () => {
                infoPanel.innerHTML = this.formatBlockInfo(block);
                blockEl.style.opacity = '0.8';
            };

            blockEl.onmouseleave = () => {
                infoPanel.innerHTML = 'Hover over a block to see details';
                blockEl.style.opacity = '1';
            };
        });
    }

    getBlockColor(domain, segment) {
        // Domain-based color palette (from MMU-MEM-UI.md)
        const domainColors = [
            '#2196F3',  // Domain 0 (kernel): Blue
            '#4CAF50',  // Domain 1: Green
            '#FF9800',  // Domain 2: Orange
            '#9C27B0',  // Domain 3: Purple
            '#F44336',  // Domain 4: Red
            '#00BCD4',  // Domain 5: Cyan
            '#FFEB3B',  // Domain 6: Yellow
            '#795548'   // Domain 7+: Brown
        ];

        if (domain < 0) return '#ECEFF1'; // Free memory: Gray

        const baseColor = domainColors[Math.min(domain, domainColors.length - 1)];

        // Adjust intensity based on segment (darker for higher segments)
        const intensity = 1.0 - (segment / 32) * 0.3;

        // Parse RGB and adjust
        const rgb = this.hexToRgb(baseColor);
        const adjusted = {
            r: Math.round(rgb.r * intensity),
            g: Math.round(rgb.g * intensity),
            b: Math.round(rgb.b * intensity)
        };

        return `rgb(${adjusted.r}, ${adjusted.g}, ${adjusted.b})`;
    }

    hexToRgb(hex) {
        const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
        return result ? {
            r: parseInt(result[1], 16),
            g: parseInt(result[2], 16),
            b: parseInt(result[3], 16)
        } : { r: 0, g: 0, b: 0 };
    }

    getBlockTitle(block) {
        if (block.domain < 0) {
            return 'Free memory';
        }

        return `Domain ${block.domain}, Segment ${block.segment}, PSN ${block.psn}`;
    }

    formatBlockInfo(block) {
        const lines = [];

        lines.push(`<div class="memory-info-row"><strong>Physical Address:</strong> ${this.formatAddr(block.phys_start)} - ${this.formatAddr(block.phys_end)}</div>`);
        lines.push(`<div class="memory-info-row"><strong>Size:</strong> ${this.formatSize(block.size)}</div>`);

        if (block.domain >= 0 && block.domain !== null && block.domain !== undefined) {
            lines.push(`<div class="memory-info-row"><strong>Owner Domain:</strong> ${block.domain}</div>`);
            lines.push(`<div class="memory-info-row"><strong>Segment:</strong> ${block.segment}</div>`);
            lines.push(`<div class="memory-info-row"><strong>PSN:</strong> ${block.psn}</div>`);
            lines.push(`<div class="memory-info-row"><strong>Mode:</strong> ${block.mode}</div>`);

            // Check if virtual address is accessible from the filtered domain
            if (this.memoryMapDomainFilter !== 'all') {
                const isAccessible = block.accessible_from_filter;
                if (isAccessible) {
                    lines.push(`<div class="memory-info-row"><strong>Virtual Address:</strong> ${this.formatAddr(block.virtual_start)}</div>`);
                    lines.push(`<div class="memory-info-row" style="color: #4CAF50;"><strong>Access:</strong> ✓ Accessible from Domain ${this.memoryMapDomainFilter}</div>`);
                } else {
                    lines.push(`<div class="memory-info-row" style="color: #999;"><strong>Virtual Address:</strong> Not accessible from Domain ${this.memoryMapDomainFilter}</div>`);
                    lines.push(`<div class="memory-info-row" style="color: #f44336;"><strong>Access:</strong> ✗ Not mapped in Domain ${this.memoryMapDomainFilter}</div>`);
                }
            } else {
                // Show virtual address when viewing all domains
                lines.push(`<div class="memory-info-row"><strong>Virtual Address:</strong> ${this.formatAddr(block.virtual_start)}</div>`);
            }

            const flags = [];
            if (block.writable) flags.push('Writable');
            if (block.public) flags.push('Public');
            if (flags.length === 0) flags.push('Read-only, Private');

            lines.push(`<div class="memory-info-row"><strong>Flags:</strong> ${flags.join(', ')}</div>`);
        } else {
            // Free/unallocated memory
            lines.push(`<div class="memory-info-row" style="color: #999;"><strong>Status:</strong> Free (unallocated)</div>`);
            lines.push(`<div class="memory-info-row" style="color: #999;"><em>Not mapped to any domain</em></div>`);
        }

        return lines.join('');
    }

    formatSize(bytes) {
        if (bytes >= 1024 * 1024) {
            return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
        } else if (bytes >= 1024) {
            return `${(bytes / 1024).toFixed(2)} KB`;
        }
        return `${bytes} bytes`;
    }

    formatAddr(addr) {
        return '0x' + addr.toString(16).padStart(8, '0').toUpperCase();
    }

    decodeProgCapability(cap) {
        // Program capability: PSN (bits 0-12), DIR (bit 15)
        const psn = cap & 0x1FFF;
        const dir = (cap & 0x8000) !== 0;
        return { psn, dir };
    }

    decodeDataCapability(cap) {
        // Data capability: PSN (bits 0-12), SHS (bit 13), PAC (bit 14), WRP (bit 15)
        const psn = cap & 0x1FFF;
        const shs = (cap & 0x2000) !== 0;
        const pac = (cap & 0x4000) !== 0;
        const wrp = (cap & 0x8000) !== 0;
        return { psn, wrp, pac, shs };
    }

    openPcbEditModal(domain, segment, progCap, dataCap) {
        if (!this.module) return;

        const modal = document.getElementById('pcbEditModal');
        const domainSpan = document.getElementById('pcbEditDomain');
        const segmentSpan = document.getElementById('pcbEditSegment');
        const domainInput = document.getElementById('pcbEditDomainInput');
        const segmentInput = document.getElementById('pcbEditSegmentInput');

        // Program capability fields
        const progTypeRadios = document.querySelectorAll('input[name="progCapType"]');
        const progDirectFields = document.getElementById('progCapDirectFields');
        const progIndirectFields = document.getElementById('progCapIndirectFields');
        const progPsnDirect = document.getElementById('pcbEditProgPsnDirect');
        const progOmc = document.getElementById('pcbEditProgOmc');
        const progDomain = document.getElementById('pcbEditProgDomain');
        const progSegment = document.getElementById('pcbEditProgSegment');
        const progRawPreview = document.getElementById('pcbEditProgRawPreview');

        // Data capability fields
        const dataPsn = document.getElementById('pcbEditDataPsn');
        const dataWrp = document.getElementById('pcbEditDataWrp');
        const dataPac = document.getElementById('pcbEditDataPac');
        const dataShs = document.getElementById('pcbEditDataShs');
        const dataRawPreview = document.getElementById('pcbEditDataRawPreview');

        const cancelBtn = document.getElementById('pcbEditCancelBtn');
        const saveBtn = document.getElementById('pcbEditSaveBtn');

        // Populate form header
        domainSpan.textContent = domain;
        segmentSpan.textContent = segment;
        domainInput.value = domain;
        segmentInput.value = segment;

        // Decode capabilities
        const progInfo = this.decodeProgCapability(progCap);
        const dataInfo = this.decodeDataCapability(dataCap);

        // Populate program capability based on type
        const bit15 = (progCap >> 15) & 1;
        if (bit15 === 0) {
            // Direct segment
            progTypeRadios[0].checked = true;
            progDirectFields.classList.remove('hidden');
            progIndirectFields.classList.add('hidden');
            progPsnDirect.value = progInfo.psn;
        } else {
            // Indirect segment
            progTypeRadios[1].checked = true;
            progDirectFields.classList.add('hidden');
            progIndirectFields.classList.remove('hidden');
            progOmc.checked = ((progCap >> 14) & 1) === 1;
            progDomain.value = (progCap >> 5) & 0xFF;
            progSegment.value = progCap & 0x1F;
        }

        // Populate data capability
        dataPsn.value = dataInfo.psn;
        dataWrp.checked = dataInfo.wrp;
        dataPac.checked = dataInfo.pac;
        dataShs.checked = dataInfo.shs;

        // Update raw previews
        const updateProgRawPreview = () => {
            let raw = 0;
            if (progTypeRadios[0].checked) {
                // Direct: bit15=0, PSN in bits 12-0
                const psn = parseInt(progPsnDirect.value || '0', 10) & 0x1FFF;
                raw = psn;
            } else {
                // Indirect: bit15=1, OMC=bit14, domain=bits 12-5, segment=bits 4-0
                raw = 0x8000; // bit 15 = 1
                if (progOmc.checked) raw |= 0x4000; // bit 14
                const dom = (parseInt(progDomain.value || '0', 10) & 0xFF) << 5;
                const seg = parseInt(progSegment.value || '0', 10) & 0x1F;
                raw |= dom | seg;
            }
            progRawPreview.value = '0x' + raw.toString(16).toUpperCase().padStart(4, '0');
        };

        const updateDataRawPreview = () => {
            let raw = 0;
            const psn = parseInt(dataPsn.value || '0', 10) & 0x1FFF;
            raw = psn;
            if (dataWrp.checked) raw |= 0x8000; // bit 15
            if (dataPac.checked) raw |= 0x4000; // bit 14
            if (dataShs.checked) raw |= 0x2000; // bit 13
            dataRawPreview.value = '0x' + raw.toString(16).toUpperCase().padStart(4, '0');
        };

        // Radio button toggle handler
        progTypeRadios.forEach(radio => {
            radio.onchange = () => {
                if (radio.value === 'direct') {
                    progDirectFields.classList.remove('hidden');
                    progIndirectFields.classList.add('hidden');
                } else {
                    progDirectFields.classList.add('hidden');
                    progIndirectFields.classList.remove('hidden');
                }
                updateProgRawPreview();
            };
        });

        // Input handlers for real-time preview
        progPsnDirect.oninput = updateProgRawPreview;
        progOmc.onchange = updateProgRawPreview;
        progDomain.oninput = updateProgRawPreview;
        progSegment.oninput = updateProgRawPreview;

        dataPsn.oninput = updateDataRawPreview;
        dataWrp.onchange = updateDataRawPreview;
        dataPac.onchange = updateDataRawPreview;
        dataShs.onchange = updateDataRawPreview;

        // Permission preset buttons
        document.querySelectorAll('.preset-btn').forEach(btn => {
            btn.onclick = (e) => {
                e.preventDefault();
                const perm = btn.dataset.perm;
                switch (perm) {
                    case 'SG_RW': // Kernel R/W: WRP=1, PAC=0
                        dataWrp.checked = true;
                        dataPac.checked = false;
                        break;
                    case 'SG_RO': // Kernel R/O: WRP=0, PAC=0
                        dataWrp.checked = false;
                        dataPac.checked = false;
                        break;
                    case 'SG_URW': // User R/W: WRP=1, PAC=1
                        dataWrp.checked = true;
                        dataPac.checked = true;
                        break;
                    case 'SG_URO': // User R/O: WRP=0, PAC=1
                        dataWrp.checked = false;
                        dataPac.checked = true;
                        break;
                }
                updateDataRawPreview();
            };
        });

        // Initial preview update
        updateProgRawPreview();
        updateDataRawPreview();

        // Tab switching handlers
        document.querySelectorAll('.pcb-tab-btn').forEach(btn => {
            btn.onclick = () => {
                const tabName = btn.dataset.tab;

                // Update tab buttons
                document.querySelectorAll('.pcb-tab-btn').forEach(b => b.classList.remove('active'));
                btn.classList.add('active');

                // Update tab content
                document.querySelectorAll('.pcb-tab-content').forEach(c => c.classList.remove('active'));
                if (tabName === 'program') {
                    document.getElementById('pcbEditProgTab').classList.add('active');
                } else if (tabName === 'data') {
                    document.getElementById('pcbEditDataTab').classList.add('active');
                }
            };
        });

        // Cancel button
        cancelBtn.onclick = () => {
            modal.classList.add('hidden');
        };

        // Save button
        saveBtn.onclick = () => {
            try {
                // Encode program capability
                let newProgCap = 0;
                if (progTypeRadios[0].checked) {
                    // Direct segment
                    const psn = parseInt(progPsnDirect.value || '0', 10) & 0x1FFF;
                    newProgCap = psn;
                } else {
                    // Indirect segment
                    newProgCap = 0x8000; // bit 15 = 1
                    if (progOmc.checked) newProgCap |= 0x4000; // bit 14
                    const dom = (parseInt(progDomain.value || '0', 10) & 0xFF) << 5;
                    const seg = parseInt(progSegment.value || '0', 10) & 0x1F;
                    newProgCap |= dom | seg;
                }

                // Encode data capability
                let newDataCap = 0;
                const psn = parseInt(dataPsn.value || '0', 10) & 0x1FFF;
                newDataCap = psn;
                if (dataWrp.checked) newDataCap |= 0x8000; // bit 15
                if (dataPac.checked) newDataCap |= 0x4000; // bit 14
                if (dataShs.checked) newDataCap |= 0x2000; // bit 13

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
            this.updateStatus('Loading demo kernel with source files...');

            // Load kernel.zip with source files
            console.log('Fetching kernel.zip...');
            const zipResponse = await fetch('kernel.zip');
            if (zipResponse.ok) {
                console.log('kernel.zip found, loading with source files...');
                const zipBlob = await zipResponse.blob();
                const zipFile = new File([zipBlob], 'kernel.zip');
                await this.handleZipUpload(zipFile);
                this.updateStatus('Demo kernel loaded with source files - NDIX-C Simulated Kernel v1.0');
                this.updateUI();

                // Automatically run mmusetup to configure MMU tables, but keep MMU disabled
                try {
                    console.log('Running automatic mmusetup...');
                    const output = this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], ['mmusetup']);
                    console.log('mmusetup completed:', output);
                    // Disable MMU so code runs at physical addresses initially
                    console.log('Disabling MMU to run at physical addresses...');
                    this.module.ccall('nd500_cmd_exec_js', 'string', ['string'], ['mmu off']);
                    this.updateStatus('Demo kernel loaded - MMU configured but disabled');
                    this.updateMmuPanel();
                } catch (mmuError) {
                    console.warn('Could not run automatic mmusetup:', mmuError);
                }

                // Source files loaded - they're available in Assembly/C tabs
                // (Keep disassembly tab active by default)

                // Set default memory view
                document.getElementById('memAddr').value = '0';
                document.getElementById('memLen').value = '256';
                this.viewMemory();
                return true;
            } else {
                // Fallback: load just the kernel binary without source files
                console.log('kernel.zip not found, loading binary only...');
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
                    this.updateStatus('Demo kernel loaded (binary only) - NDIX-C Simulated Kernel v1.0');
                    this.updateUI();
                } else {
                    throw new Error('MEMFS unavailable');
                }
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

        console.log(`[step] Before step: PC=0x${this.currentPC.toString(16)}, activeTab=${this.activeTab}`);
        this.module.ccall('nd500_dbg_step_js', null, ['number'], [1]);
        this.updateUI();
        console.log(`[step] After step: PC=0x${this.currentPC.toString(16)}`);
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
        // Clear any traps that occurred during disassembly of unmapped addresses
        // (these are expected when PC points to an unmapped address with MMU enabled)
        this.module.ccall('nd500_dbg_clear_traps_js', null, [], []);
        this.updateBreakpoints();
        this.updateMmuPanel();

        // Update source view if active (asm or c tabs)
        if (this.activeTab === 'asm' || this.activeTab === 'c') {
            this.renderSourceView();
        }
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
            console.log(`[updateDisassembly] currentPC=0x${this.currentPC.toString(16)}, instructions=${data.instructions?.length || 0}`);

            let foundCurrentPC = false;
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

                if (isCurrent) {
                    foundCurrentPC = true;
                    console.log(`[updateDisassembly] Found current PC at address 0x${lineAddr.toString(16)}`);
                }

                let classes = 'disasm-line';
                if (isCurrent) classes += ' current-pc';
                if (hasBP) classes += ' breakpoint';

                // Get source line information for this address
                const sourceInfo = this.getSourceInfoForAddr(lineAddr);
                let sourceAnnotationHtml = '';
                if (sourceInfo && sourceInfo.found) {
                    // Show just the filename, not the full path
                    const displayFilename = sourceInfo.file.split('/').pop();
                    sourceAnnotationHtml = `<div class="disasm-source-annotation"><span class="disasm-source-file">${this.escapeHtml(displayFilename)}</span><span class="disasm-source-line">:${sourceInfo.line}</span></div>`;
                }

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

                return sourceAnnotationHtml + symbolLineHtml + instructionHtml;
            }).join('');
            console.log(`[updateDisassembly] Rendered ${(data.instructions || []).length} lines, foundCurrentPC=${foundCurrentPC}`);
            if (!foundCurrentPC) {
                console.warn(`[updateDisassembly] WARNING: Current PC 0x${this.currentPC.toString(16)} not found in disassembly!`);
            }
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

    addBreakpointAtSourceLine(filename, lineNumber) {
        if (!this.module) return;

        // Get address for this source line
        const addr = this.module.ccall('nd500_dbg_addr_for_source_js', 'number', ['string', 'number'], [filename, lineNumber]);

        if (addr < 0) {
            this.updateStatus(`No code found at ${filename}:${lineNumber}`);
            return;
        }

        // Check if breakpoint already exists at this address
        const existing = this.breakpoints.find(bp => bp.addr === addr);
        if (existing) {
            this.updateStatus(`Breakpoint already exists at ${filename}:${lineNumber} (0x${addr.toString(16)})`);
            return;
        }

        // Add breakpoint
        const result = this.module.ccall('nd500_dbg_bp_add_js', 'number', ['number'], [addr]);
        if (result >= 0) {
            this.updateStatus(`Breakpoint added at ${filename}:${lineNumber} (0x${addr.toString(16)})`);
            this.updateBreakpoints();
            this.updateDisassembly();
            // Refresh the current source view to show the breakpoint marker
            this.renderSourceView();
        } else {
            this.updateStatus(`Failed to add breakpoint at ${filename}:${lineNumber}`);
        }
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

    sourceLineHasBreakpoint(filename, lineNumber) {
        if (!this.breakpoints || !this.module) return false;

        // Check if any breakpoint's address maps to this source line
        for (const bp of this.breakpoints) {
            if (!bp.enabled) continue;

            // Get source mapping for this breakpoint address
            const fileType = filename.endsWith('.c') ? 'c' : 's';
            const sourceInfo = this.getSourceMappingForAddr(bp.addr, fileType);

            if (sourceInfo && sourceInfo.found) {
                const sourceFilename = sourceInfo.file.split('/').pop();
                if (sourceFilename === filename && sourceInfo.line === lineNumber) {
                    return true;
                }
            }
        }
        return false;
    }

    /* ═══════════════════════════════════════════════════════ */
    /* SOURCE-LEVEL DEBUGGING */
    /* ═══════════════════════════════════════════════════════ */

    openSourceModal() {
        console.log('openSourceModal called');
        const modal = document.getElementById('sourceModal');
        if (!modal) {
            console.error('sourceModal element not found');
            return;
        }
        modal.classList.remove('hidden');

        // Setup modal event handlers
        const cancelBtn = document.getElementById('sourceCancelBtn');
        const uploadBtn = document.getElementById('sourceUploadBtn');

        if (cancelBtn) {
            cancelBtn.onclick = () => {
                modal.classList.add('hidden');
            };
        } else {
            console.error('sourceCancelBtn not found');
        }

        if (uploadBtn) {
            uploadBtn.onclick = () => {
                this.uploadSourceFiles();
            };
        } else {
            console.error('sourceUploadBtn not found');
        }

        // Click outside to close
        modal.onclick = (e) => {
            if (e.target === modal) {
                modal.classList.add('hidden');
            }
        };
    }

    updateFileName(elementId, file) {
        const elem = document.getElementById(elementId);
        if (file) {
            elem.textContent = file.name;
            elem.classList.add('has-file');
        } else {
            elem.textContent = 'No file selected';
            elem.classList.remove('has-file');
        }
    }

    async uploadSourceFiles() {
        const statusDiv = document.getElementById('sourceUploadStatus');
        const sourceFile = document.getElementById('sourceFileInput').files[0];
        const mapFile = document.getElementById('mapFileInput').files[0];
        const zipFile = document.getElementById('zipFileInput').files[0];

        statusDiv.classList.remove('hidden', 'success', 'error', 'info');
        statusDiv.classList.add('info');
        statusDiv.textContent = 'Uploading files...';

        try {
            if (zipFile) {
                // Handle ZIP file
                await this.handleZipUpload(zipFile);
            } else {
                // Handle individual files
                if (sourceFile) await this.handleSourceUpload(sourceFile);
                if (mapFile) await this.handleMapUpload(mapFile);
            }

            statusDiv.classList.remove('info');
            statusDiv.classList.add('success');
            statusDiv.textContent = 'Files uploaded successfully!';

            // Update source file dropdown
            this.updateSourceDropdown();

            // Source files now available in Assembly/C tabs
            // (User can manually switch tabs to view them)

            setTimeout(() => {
                document.getElementById('sourceModal').classList.add('hidden');
            }, 1500);

        } catch (error) {
            statusDiv.classList.remove('info');
            statusDiv.classList.add('error');
            statusDiv.textContent = 'Error: ' + error.message;
        }
    }

    async handleSourceUpload(file) {
        const content = await file.text();
        const fullPath = file.name;
        // Extract just the filename from path (e.g., "arithmetic/test.s" -> "test.s")
        const filename = fullPath.split('/').pop();

        console.log(`[handleSourceUpload] Processing: ${filename}`);
        console.log(`[handleSourceUpload] Content length: ${content.length} bytes`);
        console.log(`[handleSourceUpload] First 100 chars: ${content.substring(0, 100)}`);

        // Store in JS
        this.sourceFiles.set(filename, content);
        console.log(`[handleSourceUpload] Stored in sourceFiles map. Total files now: ${this.sourceFiles.size}`);

        // Store in WASM - ccall handles string conversion automatically
        const result = this.module.ccall('nd500_dbg_store_source_js', 'number', ['string', 'string'], [filename, content]);
        if (result !== 0) {
            console.error(`[handleSourceUpload] WASM storage failed for ${filename}`);
            throw new Error('Failed to store source file in WASM');
        }

        console.log(`[handleSourceUpload] Successfully uploaded: ${filename}`);
    }

    async handleMapUpload(file) {
        const content = await file.text();
        const fullPath = file.name;
        // Extract just the filename from path (e.g., "arithmetic/test.map" -> "test.map")
        const filename = fullPath.split('/').pop();

        console.log(`[handleMapUpload] Processing: ${filename}`);
        console.log(`[handleMapUpload] Content length: ${content.length} bytes`);
        console.log(`[handleMapUpload] First 300 chars: ${content.substring(0, 300)}`);

        // Write map file to WASM filesystem
        this.module.FS.writeFile(`/${filename}`, content);
        console.log(`[handleMapUpload] Written to WASM filesystem: /${filename}`);

        // Load via WASM
        const result = this.module.ccall('nd500_dbg_load_map_js', 'number', ['string'], [`/${filename}`]);
        if (result !== 0) {
            console.error(`[handleMapUpload] WASM load_map failed with result: ${result}`);
            throw new Error('Failed to load map file');
        }
        console.log(`[handleMapUpload] Map file loaded successfully into WASM`);

        // Don't change PC - the executable already set it correctly
        const currentPC = this.module.ccall('nd500_dbg_get_pc_js', 'number', [], []);
        console.log(`[handleMapUpload] Current PC: 0x${currentPC.toString(16)} (not changing)`);

        console.log(`[handleMapUpload] Successfully uploaded map file: ${filename}`);
    }

    async handleZipUpload(file) {
        const JSZip = window.JSZip;
        if (!JSZip) {
            throw new Error('JSZip library not loaded');
        }

        console.log(`[handleZipUpload] Starting ZIP extraction: ${file.name}`);
        this.updateStatus(`Extracting ZIP: ${file.name}...`);
        const zip = await JSZip.loadAsync(file);
        console.log(`[handleZipUpload] ZIP file contents:`, Object.keys(zip.files));

        let counts = { source: 0, map: 0, executable: 0, skipped: 0 };

        // Process files in ZIP
        for (const [filename, zipEntry] of Object.entries(zip.files)) {
            if (zipEntry.dir) {
                console.log(`[handleZipUpload] Skipping directory: ${filename}`);
                continue;
            }

            // Extract basename (without path)
            const basename = filename.split('/').pop();
            console.log(`[handleZipUpload] Processing file: ${filename} -> basename: ${basename}`);

            // Read each file in its appropriate format
            if (filename.endsWith('.s') || filename.endsWith('.asm') || filename.endsWith('.c')) {
                // Source file (.s, .asm, .c) - read as text
                console.log(`[handleZipUpload] Detected as SOURCE file: ${basename}`);
                const content = await zipEntry.async('string');
                await this.handleSourceUpload(new File([content], basename));
                counts.source++;
            } else if (filename.endsWith('.map')) {
                // Map file - read as text
                console.log(`[handleZipUpload] Detected as MAP file: ${basename}`);
                const content = await zipEntry.async('string');
                await this.handleMapUpload(new File([content], basename));
                counts.map++;
            } else if (filename.endsWith('.o') || filename.endsWith('.out') ||
                       (!filename.includes('.') && basename !== '__MACOSX' && !basename.startsWith('.'))) {
                // Binary executable file:
                // - .o or .out extension
                // - No extension at all (e.g., "kernel", "program")
                // - Skip macOS metadata and hidden files
                console.log(`[handleZipUpload] Detected as EXECUTABLE file: ${basename}`);
                const bytes = await zipEntry.async('uint8array');
                await this.loadAoutFromBuffer(bytes, basename);
                counts.executable++;
            } else {
                console.log(`[handleZipUpload] SKIPPING unrecognized file: ${filename}`);
                counts.skipped++;
            }
        }

        console.log(`[handleZipUpload] Processing complete. Counts:`, counts);
        console.log(`[handleZipUpload] sourceFiles map now has ${this.sourceFiles.size} entries:`, Array.from(this.sourceFiles.keys()));

        // Update source dropdowns after all files loaded
        console.log(`[handleZipUpload] Calling updateSourceDropdown()`);
        this.updateSourceDropdown();

        // Show summary
        const parts = [];
        if (counts.executable > 0) parts.push(`${counts.executable} executable(s)`);
        if (counts.source > 0) parts.push(`${counts.source} source file(s)`);
        if (counts.map > 0) parts.push(`${counts.map} map file(s)`);
        const summary = parts.length > 0 ? parts.join(', ') : 'no recognized files';
        console.log(`[handleZipUpload] Final summary: ${summary}`);
        this.updateStatus(`ZIP loaded: ${summary}`);
    }

    async loadAoutFromBuffer(bytes, fullPath) {
        // Extract just the filename from path (e.g., "arithmetic/test.o" -> "test.o")
        const filename = fullPath.split('/').pop();

        this.updateStatus(`Loading executable: ${filename}...`);

        // Write to WASM filesystem
        this.module.FS.writeFile(`/${filename}`, bytes);

        // Load via WASM
        const result = this.module.ccall('nd500_dbg_load_aout_path_js', 'number', ['string'], [`/${filename}`]);
        if (result !== 0) {
            console.warn(`Failed to load executable: ${filename}`);
            this.updateStatus(`Failed to load executable: ${filename}`);
        } else {
            console.log(`Loaded executable: ${filename}`);
            this.updateStatus(`Loaded executable: ${filename}`);
            this.updateUI();
        }
    }

    updateSourceDropdown() {
        console.log(`[updateSourceDropdown] Called. sourceFiles has ${this.sourceFiles.size} entries`);

        // Update both assembly and C dropdowns separately
        const asmDropdown = document.getElementById('asm-file-dropdown');
        const cDropdown = document.getElementById('c-file-dropdown');

        if (!asmDropdown) {
            console.error(`[updateSourceDropdown] asm-file-dropdown element not found!`);
            return;
        }
        if (!cDropdown) {
            console.error(`[updateSourceDropdown] c-file-dropdown element not found!`);
            return;
        }

        asmDropdown.innerHTML = '';
        cDropdown.innerHTML = '';

        let hasAsm = false;
        let hasC = false;

        // Separate files by extension
        for (const filename of this.sourceFiles.keys()) {
            const ext = filename.substring(filename.lastIndexOf('.'));
            console.log(`[updateSourceDropdown] Processing: ${filename}, extension: ${ext}`);

            if (ext === '.s') {
                console.log(`[updateSourceDropdown] Adding to ASM dropdown: ${filename}`);
                const option = document.createElement('option');
                option.value = filename;
                option.textContent = filename;
                asmDropdown.appendChild(option);
                hasAsm = true;
            } else if (ext === '.c') {
                console.log(`[updateSourceDropdown] Adding to C dropdown: ${filename}`);
                const option = document.createElement('option');
                option.value = filename;
                option.textContent = filename;
                cDropdown.appendChild(option);
                hasC = true;
            } else {
                console.log(`[updateSourceDropdown] Unrecognized extension for: ${filename}`);
            }
        }

        // Set default options if no files
        if (!hasAsm) {
            console.log(`[updateSourceDropdown] No ASM files found, setting default message`);
            asmDropdown.innerHTML = '<option value="">-- No assembly files loaded --</option>';
        } else {
            console.log(`[updateSourceDropdown] Added ${asmDropdown.options.length} ASM files to dropdown`);
            // Auto-select first option
            if (asmDropdown.options.length > 0) {
                asmDropdown.selectedIndex = 0;
                console.log(`[updateSourceDropdown] Auto-selected ASM file: ${asmDropdown.value}`);
            }
        }
        if (!hasC) {
            console.log(`[updateSourceDropdown] No C files found, setting default message`);
            cDropdown.innerHTML = '<option value="">-- No C files loaded --</option>';
        } else {
            console.log(`[updateSourceDropdown] Added ${cDropdown.options.length} C files to dropdown`);
            // Auto-select first option
            if (cDropdown.options.length > 0) {
                cDropdown.selectedIndex = 0;
                console.log(`[updateSourceDropdown] Auto-selected C file: ${cDropdown.value}`);
            }
        }

        // Render source views after updating dropdowns
        console.log(`[updateSourceDropdown] Calling renderSourceView() to display content`);
        this.renderSourceView();
    }

    switchCodeTab(tabName) {
        this.activeTab = tabName;

        // Update tab buttons
        document.querySelectorAll('.code-tab').forEach(tab => {
            if (tab.dataset.tab === tabName) {
                tab.classList.add('active');
            } else {
                tab.classList.remove('active');
            }
        });

        // Update panels
        document.querySelectorAll('.code-panel').forEach(panel => {
            panel.classList.remove('active');
        });

        if (tabName === 'disasm') {
            document.getElementById('disasm-panel').classList.add('active');
        } else if (tabName === 'asm') {
            document.getElementById('asm-panel').classList.add('active');
            // Update registers to ensure currentPC is current
            this.updateRegisters();
            this.renderAsmSourceView();
        } else if (tabName === 'c') {
            document.getElementById('c-panel').classList.add('active');
            // Update registers to ensure currentPC is current
            this.updateRegisters();
            this.renderCSourceView();
        }
    }

    renderSourceView() {
        console.log(`[renderSourceView] Called. activeTab=${this.activeTab}, currentPC=0x${this.currentPC.toString(16)}`);
        // Legacy function - redirects to appropriate source view based on active tab
        if (this.activeTab === 'asm') {
            this.renderAsmSourceView();
        } else if (this.activeTab === 'c') {
            this.renderCSourceView();
        }
    }

    renderAsmSourceView() {
        console.log(`[renderAsmSourceView] Called. currentPC=0x${this.currentPC.toString(16)}`);
        const contentDiv = document.getElementById('asm-content');
        const dropdown = document.getElementById('asm-file-dropdown');

        // Get selected file from dropdown
        let currentFile = dropdown.value;

        // If no file selected, try to auto-select based on PC
        if (!currentFile && this.sourceFiles.size > 0) {
            const pc = this.currentPC;
            const sourceInfo = this.getSourceMappingForAddr(pc, 's');
            if (sourceInfo && sourceInfo.found) {
                currentFile = sourceInfo.file.split('/').pop();
                dropdown.value = currentFile;
            }
        }

        if (!currentFile || !this.sourceFiles.has(currentFile)) {
            console.log(`[renderAsmSourceView] No file selected or not in sourceFiles`);
            contentDiv.innerHTML = '<div class="source-empty-state">No assembly file selected.<br>Click "Source Files" to upload .s files.</div>';
            return;
        }

        const content = this.sourceFiles.get(currentFile);
        const lines = content.split('\n');

        // Get current PC and find matching source line
        const pc = this.currentPC;
        const sourceInfo = this.getSourceMappingForAddr(pc, 's');
        let currentLine = -1;

        if (sourceInfo && sourceInfo.found) {
            const sourceFilename = sourceInfo.file.split('/').pop();
            if (sourceFilename === currentFile) {
                currentLine = sourceInfo.line;
            }
        }
        console.log(`[renderAsmSourceView] File=${currentFile}, currentLine=${currentLine}, totalLines=${lines.length}`);

        // Render lines
        let html = '';
        lines.forEach((line, index) => {
            const lineNumber = index + 1;
            const isCurrent = lineNumber === currentLine;
            const hasBreakpoint = this.sourceLineHasBreakpoint(currentFile, lineNumber);
            const classes = ['source-line'];
            if (isCurrent) classes.push('current-line');
            if (hasBreakpoint) classes.push('has-breakpoint');

            html += `<div class="${classes.join(' ')}" data-line="${lineNumber}" data-filename="${currentFile}">`;
            html += `<span class="source-line-number">${lineNumber}</span>`;
            html += `<span class="source-line-content">${this.escapeHtml(line)}</span>`;
            html += '</div>';
        });

        contentDiv.innerHTML = html;

        // Add click handlers to each line for setting breakpoints
        contentDiv.querySelectorAll('.source-line').forEach(lineElem => {
            lineElem.addEventListener('click', (e) => {
                const lineNum = parseInt(lineElem.getAttribute('data-line'));
                const filename = lineElem.getAttribute('data-filename');
                this.addBreakpointAtSourceLine(filename, lineNum);
            });
        });

        // Scroll to current line
        if (currentLine >= 0) {
            const currentLineElem = contentDiv.querySelector(`.source-line[data-line="${currentLine}"]`);
            if (currentLineElem) {
                currentLineElem.scrollIntoView({ block: 'center', behavior: 'smooth' });
            }
        }
    }

    renderCSourceView() {
        console.log(`[renderCSourceView] Called. currentPC=0x${this.currentPC.toString(16)}`);
        const contentDiv = document.getElementById('c-content');
        const dropdown = document.getElementById('c-file-dropdown');

        // Get selected file from dropdown
        let currentFile = dropdown.value;

        // If no file selected, try to auto-select based on PC
        if (!currentFile && this.sourceFiles.size > 0) {
            const pc = this.currentPC;
            const sourceInfo = this.getSourceMappingForAddr(pc, 'c');
            if (sourceInfo && sourceInfo.found) {
                currentFile = sourceInfo.file.split('/').pop();
                dropdown.value = currentFile;
            }
        }

        if (!currentFile || !this.sourceFiles.has(currentFile)) {
            console.log(`[renderCSourceView] No file selected or not in sourceFiles`);
            contentDiv.innerHTML = '<div class="source-empty-state">No C file selected.<br>Click "Source Files" to upload .c files.</div>';
            return;
        }

        const content = this.sourceFiles.get(currentFile);
        const lines = content.split('\n');

        // Get current PC and find matching source line
        const pc = this.currentPC;
        const sourceInfo = this.getSourceMappingForAddr(pc, 'c');
        let currentLine = -1;

        if (sourceInfo && sourceInfo.found) {
            const sourceFilename = sourceInfo.file.split('/').pop();
            if (sourceFilename === currentFile) {
                currentLine = sourceInfo.line;
            }
        }
        console.log(`[renderCSourceView] File=${currentFile}, currentLine=${currentLine}, totalLines=${lines.length}`);

        // Render lines
        let html = '';
        lines.forEach((line, index) => {
            const lineNumber = index + 1;
            const isCurrent = lineNumber === currentLine;
            const hasBreakpoint = this.sourceLineHasBreakpoint(currentFile, lineNumber);
            const classes = ['source-line'];
            if (isCurrent) classes.push('current-line');
            if (hasBreakpoint) classes.push('has-breakpoint');

            html += `<div class="${classes.join(' ')}" data-line="${lineNumber}" data-filename="${currentFile}">`;
            html += `<span class="source-line-number">${lineNumber}</span>`;
            html += `<span class="source-line-content">${this.escapeHtml(line)}</span>`;
            html += '</div>';
        });

        contentDiv.innerHTML = html;

        // Add click handlers to each line for setting breakpoints
        contentDiv.querySelectorAll('.source-line').forEach(lineElem => {
            lineElem.addEventListener('click', (e) => {
                const lineNum = parseInt(lineElem.getAttribute('data-line'));
                const filename = lineElem.getAttribute('data-filename');
                this.addBreakpointAtSourceLine(filename, lineNum);
            });
        });

        // Scroll to current line
        if (currentLine >= 0) {
            const currentLineElem = contentDiv.querySelector(`.source-line[data-line="${currentLine}"]`);
            if (currentLineElem) {
                currentLineElem.scrollIntoView({ block: 'center', behavior: 'smooth' });
            }
        }
    }

    getSourceMappingForAddr(addr, fileType) {
        // fileType: 's' for assembly, 'c' for C
        if (!this.module) return { found: false };

        try {
            // Use file-type-specific WASM function
            const funcName = fileType === 'c' ?
                'nd500_dbg_source_c_mapping_json_js' :
                'nd500_dbg_source_s_mapping_json_js';

            console.log(`[getSourceMappingForAddr] addr=0x${addr.toString(16)}, fileType=${fileType}, calling ${funcName}`);

            const json = this.module.ccall(funcName, 'string', ['number'], [addr]);
            const info = JSON.parse(json);

            console.log(`[getSourceMappingForAddr] WASM returned:`, info);

            if (!info || !info.found) {
                console.log(`[getSourceMappingForAddr] Not found for type ${fileType}`);
                return { found: false };
            }

            console.log(`[getSourceMappingForAddr] Found! Returning ${info.file}:${info.line}`);
            return info;
        } catch (error) {
            console.error('Error getting source mapping:', error);
            return { found: false };
        }
    }

    getSourceInfoForAddr(addr) {
        if (!this.module) return { found: false };

        try {
            const json = this.module.ccall('nd500_dbg_source_info_json_js', 'string', ['number'], [addr]);
            return JSON.parse(json);
        } catch (error) {
            console.error('Error getting source info:', error);
            return { found: false };
        }
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
            console.log(`[ConsoleManager] Executing command: "${cmdline}"`);
            const output = this.dbg.module.ccall('nd500_cmd_exec_js', 'string', ['string'], [cmdline]);
            console.log(`[ConsoleManager] Command output length: ${output ? output.length : 0}`);
            console.log(`[ConsoleManager] Command output:`, output);

            if (output && output.trim()) {
                // Split output into lines and add each
                const lines = output.split('\n');
                console.log(`[ConsoleManager] Output has ${lines.length} lines`);
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
            } else {
                console.log(`[ConsoleManager] No output to display (empty or whitespace only)`);
                this.addLine('(no output)', 'output');
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
