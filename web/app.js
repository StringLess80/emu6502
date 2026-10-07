/*
 * app.js - The web interface of emu6502.
 *
 * The emulator itself is the C core compiled to WebAssembly (emu-wasm.js).
 * This file only draws the terminal and the panels, reads the keyboard,
 * loads files, and decides how many cycles to run on each video frame.
 */
"use strict";

(async function () {
  const $ = (sel) => document.querySelector(sel);
  const hex2 = (n) => n.toString(16).toUpperCase().padStart(2, "0");
  const hex4 = (n) => n.toString(16).toUpperCase().padStart(4, "0");

  /* ------------------------------------------------------------------ */
  /* The WebAssembly core                                                */
  /* ------------------------------------------------------------------ */

  const wasmBytes = Uint8Array.from(atob(window.EMU_WASM_BASE64), (c) => c.charCodeAt(0));
  const { instance } = await WebAssembly.instantiate(wasmBytes, {});
  const E = instance.exports;
  const mem = () => new Uint8Array(E.memory.buffer);

  const REG = { A: 0, X: 1, Y: 2, SP: 3, P: 4, PC: 5, HALTED: 6 };
  const DEV = {
    ORA: 0, ORB: 1, DDRA: 2, DDRB: 3, PORTA: 4, PORTB: 5,
    T1: 6, T1LATCH: 7, T2: 8, ACR: 9, PCR: 10, IFR: 11, IER: 12,
    ACIA_STATUS: 13, ACIA_COMMAND: 14, ACIA_CONTROL: 15,
  };
  const RUN = { OK: 0, BREAKPOINT: 1, HALTED: 2 };

  function cString(ptr) {
    if (!ptr) return null;
    const m = mem();
    let s = "";
    for (let i = ptr; m[i] !== 0; i++) s += String.fromCharCode(m[i]);
    return s;
  }

  /* The opcode table, read once from the C side. */
  const OPS = [];
  for (let op = 0; op < 256; op++) {
    OPS.push({ name: cString(E.emu_op_name(op)), mode: E.emu_op_mode(op), len: E.emu_op_len(op) });
  }

  const MODE = { IMP: 0, ACC: 1, IMM: 2, ZP: 3, ZPX: 4, ZPY: 5, ABS: 6, ABSX: 7, ABSY: 8, IND: 9, INDX: 10, INDY: 11, REL: 12 };

  function disassemble(addr) {
    const op = E.emu_peek(addr);
    const info = OPS[op];
    if (!info.name) return { len: 1, bytes: [op], text: ".BYTE $" + hex2(op) };
    const lo = E.emu_peek((addr + 1) & 0xffff);
    const hi = E.emu_peek((addr + 2) & 0xffff);
    const word = lo | (hi << 8);
    const n = info.name;
    let text;
    switch (info.mode) {
      case MODE.IMP:  text = n; break;
      case MODE.ACC:  text = n + " A"; break;
      case MODE.IMM:  text = `${n} #$${hex2(lo)}`; break;
      case MODE.ZP:   text = `${n} $${hex2(lo)}`; break;
      case MODE.ZPX:  text = `${n} $${hex2(lo)},X`; break;
      case MODE.ZPY:  text = `${n} $${hex2(lo)},Y`; break;
      case MODE.ABS:  text = `${n} $${hex4(word)}`; break;
      case MODE.ABSX: text = `${n} $${hex4(word)},X`; break;
      case MODE.ABSY: text = `${n} $${hex4(word)},Y`; break;
      case MODE.IND:  text = `${n} ($${hex4(word)})`; break;
      case MODE.INDX: text = `${n} ($${hex2(lo)},X)`; break;
      case MODE.INDY: text = `${n} ($${hex2(lo)}),Y`; break;
      case MODE.REL: {
        const offset = lo < 128 ? lo : lo - 256;
        text = `${n} $${hex4((addr + 2 + offset) & 0xffff)}`;
        break;
      }
      default: text = n;
    }
    const bytes = [op, lo, hi].slice(0, info.len);
    return { len: info.len, bytes, text };
  }

  /* ------------------------------------------------------------------ */
  /* Terminal                                                            */
  /* ------------------------------------------------------------------ */

  class Terminal {
    constructor(historyEl, gridEl, cols, rows) {
      this.historyEl = historyEl;
      this.gridEl = gridEl;
      this.cols = cols;
      this.rows = rows;
      this.historyLines = 0;
      this.clear();
    }

    clear() {
      this.grid = Array.from({ length: this.rows }, () => new Array(this.cols).fill(" "));
      this.cx = 0;
      this.cy = 0;
      this.lastWasCR = false;
      this.esc = null;      // null, "esc" or the CSI parameters collected so far
      this.dirty = true;
    }

    clearHistory() {
      this.historyEl.textContent = "";
      this.historyLines = 0;
    }

    newline() {
      this.cx = 0;
      if (this.cy < this.rows - 1) {
        this.cy++;
        return;
      }
      // Scroll: the top line goes into the history above the screen.
      const top = this.grid.shift().join("").replace(/\s+$/, "");
      this.historyEl.appendChild(document.createTextNode(top + "\n"));
      if (++this.historyLines > 2000) {
        this.historyEl.removeChild(this.historyEl.firstChild);
        this.historyLines--;
      }
      this.grid.push(new Array(this.cols).fill(" "));
    }

    csi(params, final) {
      const nums = params.split(";").map((p) => parseInt(p, 10));
      const n = (i, dflt) => (Number.isNaN(nums[i]) || nums[i] === undefined ? dflt : nums[i]);
      switch (final) {
        case "J":
          if (n(0, 0) === 2) {
            for (const row of this.grid) row.fill(" ");
          } else {
            for (let x = this.cx; x < this.cols; x++) this.grid[this.cy][x] = " ";
            for (let y = this.cy + 1; y < this.rows; y++) this.grid[y].fill(" ");
          }
          break;
        case "K":
          for (let x = this.cx; x < this.cols; x++) this.grid[this.cy][x] = " ";
          break;
        case "H":
        case "f":
          this.cy = Math.min(this.rows - 1, Math.max(0, n(0, 1) - 1));
          this.cx = Math.min(this.cols - 1, Math.max(0, n(1, 1) - 1));
          break;
        case "A": this.cy = Math.max(0, this.cy - n(0, 1)); break;
        case "B": this.cy = Math.min(this.rows - 1, this.cy + n(0, 1)); break;
        case "C": this.cx = Math.min(this.cols - 1, this.cx + n(0, 1)); break;
        case "D": this.cx = Math.max(0, this.cx - n(0, 1)); break;
        default: break; // colours and other sequences are ignored
      }
    }

    put(byte) {
      this.dirty = true;
      const ch = String.fromCharCode(byte & 0x7f);

      if (this.esc === "esc") {
        this.esc = ch === "[" ? "" : null;
        return;
      }
      if (this.esc !== null) {
        if (/[0-9;?]/.test(ch)) {
          this.esc += ch;
        } else {
          this.csi(this.esc, ch);
          this.esc = null;
        }
        return;
      }

      const b = byte & 0x7f;
      const wasCR = this.lastWasCR;
      this.lastWasCR = false;

      if (b === 0x0d) {            // CR: WozMon ends lines with CR alone
        this.newline();
        this.lastWasCR = true;
      } else if (b === 0x0a) {     // LF right after a CR was already done
        if (!wasCR) this.newline();
      } else if (b === 0x08) {
        if (this.cx > 0) this.cx--;
      } else if (b === 0x1b) {
        this.esc = "esc";
      } else if (b >= 0x20 && b < 0x7f) {
        this.grid[this.cy][this.cx] = ch;
        if (++this.cx >= this.cols) this.newline();
      }
      // other control characters (bell, form feed...) are ignored
    }

    write(bytes) {
      for (const b of bytes) this.put(b);
    }

    render() {
      if (!this.dirty) return;
      this.dirty = false;
      const esc = (s) => s.replace(/&/g, "&amp;").replace(/</g, "&lt;");
      const lines = this.grid.map((row, y) => {
        if (y !== this.cy) return esc(row.join(""));
        const before = row.slice(0, this.cx).join("");
        const at = row[this.cx] || " ";
        const after = row.slice(this.cx + 1).join("");
        return `${esc(before)}<span class="cursor">${esc(at)}</span>${esc(after)}`;
      });
      this.gridEl.innerHTML = lines.join("\n");
    }
  }

  const termEl = $("#term");
  const term = new Terminal($("#term-history"), $("#term-grid"), 80, 24);

  // Choose the font size so that 80 columns fill the screen's width.
  function fitFont() {
    const probe = document.createElement("span");
    probe.textContent = "M".repeat(80);
    probe.style.cssText = "position:absolute;visibility:hidden;white-space:pre;font-size:20px";
    termEl.appendChild(probe);
    const widthAt20 = probe.getBoundingClientRect().width;
    probe.remove();
    const style = getComputedStyle(termEl);
    const inner = termEl.clientWidth - parseFloat(style.paddingLeft) - parseFloat(style.paddingRight) - 4;
    const size = Math.max(11, Math.min(24, (20 * inner) / widthAt20));
    termEl.style.fontSize = `${size.toFixed(2)}px`;
  }
  fitFont();
  window.addEventListener("resize", fitFont);
  if (document.fonts) document.fonts.ready.then(fitFont);

  // Keep the newest output in view, unless the user scrolled up to read
  // older lines.
  let followOutput = true;
  termEl.addEventListener("scroll", () => {
    followOutput = termEl.scrollHeight - termEl.scrollTop - termEl.clientHeight < 40;
  });

  function drainOutput() {
    const len = E.emu_out_len();
    if (len === 0) return false;
    const ptr = E.emu_out_ptr();
    term.write(mem().subarray(ptr, ptr + len));
    E.emu_out_clear();
    return true;
  }

  /* Keyboard: every key goes to the 6502 through the ACIA. */

  const upcaseEl = $("#upcase");

  function keyCode(ch) {
    if (upcaseEl.checked) ch = ch.toUpperCase();
    const c = ch.charCodeAt(0);
    return c < 128 ? c : null;
  }

  termEl.addEventListener("keydown", (e) => {
    if (e.metaKey) return;                        // let Cmd+C / Cmd+V work
    if (e.ctrlKey && (e.key === "v" || e.key === "V")) return;   // paste
    let code = null;
    if (e.ctrlKey && !e.altKey && e.key.length === 1) {
      const k = e.key.toUpperCase().charCodeAt(0);
      if (k >= 0x40 && k <= 0x5f) code = k - 0x40;   // Ctrl+C is 3, etc.
    } else if (e.key === "Enter") code = 0x0d;
    else if (e.key === "Backspace") code = 0x08;
    else if (e.key === "Escape") code = 0x1b;
    else if (e.key === "Tab") code = 0x09;
    else if (e.key.length === 1 && !e.altKey) code = keyCode(e.key);
    if (code !== null) {
      E.emu_key(code);
      e.preventDefault();
    }
  });

  termEl.addEventListener("paste", (e) => {
    const text = (e.clipboardData || window.clipboardData).getData("text");
    for (const ch of text.replace(/\r\n?/g, "\n")) {
      const code = ch === "\n" ? 0x0d : keyCode(ch);
      if (code !== null) E.emu_key(code);
    }
    e.preventDefault();
  });

  /* ------------------------------------------------------------------ */
  /* Running                                                             */
  /* ------------------------------------------------------------------ */

  const state = {
    hasRom: false,
    running: false,
    speed: 1000000,
    skipBreakpoint: false,  // let "continue" leave the breakpoint it sits on
    tempBreak: null,        // step over: the address after the JSR
    message: "",
    messageKind: "",
    lastFrame: 0,
    cycleCarry: 0,
    mhzSample: { t: 0, cycles: 0 },
    mhz: 0,
    panelTimer: 0,
  };

  const bpTable = () => mem().subarray(E.emu_breakpoints(), E.emu_breakpoints() + 65536);

  function setStatus(text, kind = "") {
    state.message = text;
    state.messageKind = kind;
  }

  function setRunning(on) {
    if (on && !state.hasRom) return;
    if (on && E.emu_reg(REG.HALTED)) return;
    state.running = on;
    if (on) {
      state.skipBreakpoint = true;
      state.lastFrame = performance.now();
      state.cycleCarry = 0;
      setStatus("Running");
    } else if (!state.message.startsWith("Breakpoint") && !state.message.startsWith("Halted")) {
      setStatus(`Paused at $${hex4(E.emu_reg(REG.PC))}`, "stopped");
    }
    updateButtons();
  }

  function handleRunResult(result) {
    if (result === RUN.BREAKPOINT) {
      const pc = E.emu_reg(REG.PC);
      state.running = false;
      if (state.tempBreak === pc) {
        clearTempBreak();
        setStatus(`Stepped over, at $${hex4(pc)}`, "stopped");
      } else {
        setStatus(`Breakpoint at $${hex4(pc)}`, "stopped");
      }
      updateButtons();
    } else if (result === RUN.HALTED) {
      haltedMessage();
    }
  }

  function haltedMessage() {
    const pc = E.emu_reg(REG.PC);
    state.running = false;
    setStatus(`Halted: illegal opcode $${hex2(E.emu_peek(pc))} at $${hex4(pc)}. Press Reset.`, "error");
    updateButtons();
  }

  function runFrame(now) {
    const dt = Math.min(now - state.lastFrame, 100) / 1000;
    state.lastFrame = now;
    let result = RUN.OK;

    if (state.speed === 0) {
      const t0 = performance.now();
      do {
        result = E.emu_run(250000, state.skipBreakpoint ? 1 : 0);
        state.skipBreakpoint = false;
      } while (result === RUN.OK && performance.now() - t0 < 14);
    } else {
      const want = state.speed * dt + state.cycleCarry;
      const cycles = Math.floor(want);
      state.cycleCarry = want - cycles;
      if (cycles > 0) {
        result = E.emu_run(cycles, state.skipBreakpoint ? 1 : 0);
        state.skipBreakpoint = false;
      }
    }
    handleRunResult(result);
  }

  function frame(now) {
    if (state.running) runFrame(now);
    if (drainOutput() || term.dirty) {
      term.render();
      if (followOutput) termEl.scrollTop = termEl.scrollHeight;
    }

    // Panels: every frame while paused, ten times a second while running.
    if (!state.running || now - state.panelTimer > 100) {
      state.panelTimer = now;
      updatePanels();
    }
    requestAnimationFrame(frame);
  }

  function step() {
    if (!state.hasRom) return;
    if (state.running) setRunning(false);
    if (E.emu_reg(REG.HALTED)) return haltedMessage();
    E.emu_step();
    if (E.emu_reg(REG.HALTED)) haltedMessage();
    else setStatus(`Stepped to $${hex4(E.emu_reg(REG.PC))}`, "stopped");
    updatePanels(true);
  }

  function clearTempBreak() {
    if (state.tempBreak !== null) {
      if (!breakpoints.has(state.tempBreak)) bpTable()[state.tempBreak] = 0;
      state.tempBreak = null;
    }
  }

  function stepOver() {
    if (!state.hasRom) return;
    const pc = E.emu_reg(REG.PC);
    if (E.emu_peek(pc) !== 0x20) return step();    // not a JSR: plain step
    clearTempBreak();
    state.tempBreak = (pc + 3) & 0xffff;
    bpTable()[state.tempBreak] = 1;
    setRunning(true);
  }

  /* ------------------------------------------------------------------ */
  /* Breakpoints                                                         */
  /* ------------------------------------------------------------------ */

  const breakpoints = new Set();

  function toggleBreakpoint(addr) {
    if (breakpoints.has(addr)) {
      breakpoints.delete(addr);
      if (state.tempBreak !== addr) bpTable()[addr] = 0;
    } else {
      breakpoints.add(addr);
      bpTable()[addr] = 1;
    }
    renderBreakpointList();
    updatePanels(true);
  }

  function renderBreakpointList() {
    const box = $("#bp-list");
    box.innerHTML = "";
    [...breakpoints].sort((a, b) => a - b).forEach((addr) => {
      const b = document.createElement("button");
      b.className = "chip";
      b.textContent = `$${hex4(addr)} ×`;
      b.title = "Remove this breakpoint";
      b.addEventListener("click", () => toggleBreakpoint(addr));
      box.appendChild(b);
    });
  }

  $("#bp-addr").addEventListener("keydown", (e) => {
    if (e.key !== "Enter") return;
    const addr = parseInt(e.target.value, 16);
    if (!Number.isNaN(addr) && addr >= 0 && addr <= 0xffff) {
      if (!breakpoints.has(addr)) toggleBreakpoint(addr);
      e.target.value = "";
    }
  });

  /* ------------------------------------------------------------------ */
  /* Panels                                                              */
  /* ------------------------------------------------------------------ */

  const prev = { regs: {}, mem: null, memAddr: -1 };
  const FLAG_NAMES = [
    ["N", "neg"], ["V", "ovf"], ["-", ""], ["B", "brk"],
    ["D", "dec"], ["I", "irq"], ["Z", "zero"], ["C", "carry"],
  ];

  // Build the static parts once.
  const flagsEl = $("#flags");
  const flagEls = FLAG_NAMES.map(([letter, label]) => {
    const el = document.createElement("div");
    el.className = letter === "-" ? "flag is-unused" : "flag";
    if (letter === "-") el.title = "Unused bit: always 1";
    el.innerHTML = `${letter}<span class="flag-label">${label || "&nbsp;"}</span>`;
    flagsEl.appendChild(el);
    return el;
  });

  function makeLeds(container) {
    const leds = [];
    for (let bit = 7; bit >= 0; bit--) {
      const led = document.createElement("span");
      led.className = "led";
      led.title = `Bit ${bit}`;
      container.appendChild(led);
      leds.push(led);
    }
    return leds;
  }
  const ledsA = makeLeds($("#led-a"));
  const ledsB = makeLeds($("#led-b"));

  function setReg(id, key, text) {
    const el = $(id);
    if (el.textContent !== text) {
      el.textContent = text;
      el.classList.toggle("changed", prev.regs[key] !== undefined);
    } else if (!state.running) {
      el.classList.remove("changed");
    }
    prev.regs[key] = text;
  }

  let disStart = 0;

  function updateDisassembly(pc) {
    // Keep the listing still while the PC moves inside it; restart it at
    // the PC when the PC leaves it.
    const lines = [];
    let addr = disStart;
    let pcInside = false;
    for (let i = 0; i < 18; i++) {
      const d = disassemble(addr);
      if (addr === pc && i < 14) pcInside = true;
      lines.push({ addr, d });
      addr = (addr + d.len) & 0xffff;
    }
    if (!pcInside) {
      disStart = pc;
      return updateDisassembly(pc);
    }

    const bps = bpTable();
    const ol = $("#dis");
    ol.innerHTML = "";
    for (const { addr: a, d } of lines) {
      const li = document.createElement("li");
      if (a === pc) li.classList.add("is-pc");
      if (bps[a] && a !== state.tempBreak) li.classList.add("has-bp");
      li.innerHTML =
        `<span class="bp-dot">●</span><span class="addr">${hex4(a)}</span>` +
        `<span class="bytes">${d.bytes.map(hex2).join(" ")}</span><span>${d.text}</span>`;
      li.title = `Toggle a breakpoint at $${hex4(a)}`;
      li.addEventListener("click", () => toggleBreakpoint(a));
      ol.appendChild(li);
    }
  }

  function updateStack(sp) {
    const ol = $("#stack");
    ol.innerHTML = "";
    const depth = 0xff - sp;
    $("#stack-depth").textContent = depth === 1 ? "1 byte" : `${depth} bytes`;
    if (depth === 0) {
      ol.innerHTML = `<li><span class="empty">Empty</span></li>`;
      return;
    }
    const rows = Math.min(depth, 14);
    for (let i = 1; i <= rows; i++) {
      const addr = 0x100 + sp + i;
      const value = E.emu_peek(addr);
      let note = "";
      // A JSR pushes its own last byte's address: low byte on top.
      if (addr < 0x1ff) {
        const target = value | (E.emu_peek(addr + 1) << 8);
        const call = (target - 2) & 0xffff;
        if (E.emu_peek(call) === 0x20) note = `returns to $${hex4((target + 1) & 0xffff)}`;
      }
      const li = document.createElement("li");
      if (i === 1) li.classList.add("is-top");
      li.innerHTML = `<span class="addr">$${hex4(addr)}</span><span>${hex2(value)}</span><span class="note">${note}</span>`;
      ol.appendChild(li);
    }
  }

  let memAddr = 0x0000;
  const MEM_ROWS = 16;

  function updateMemory(sp) {
    const len = MEM_ROWS * 16;
    const ptr = E.emu_peek_block(memAddr, len);
    const bytes = mem().slice(ptr, ptr + len);
    const old = prev.memAddr === memAddr ? prev.mem : null;

    let html = "";
    for (let r = 0; r < MEM_ROWS; r++) {
      const rowAddr = (memAddr + r * 16) & 0xffff;
      let hexPart = "";
      let ascii = "";
      for (let c = 0; c < 16; c++) {
        const i = r * 16 + c;
        const v = bytes[i];
        const a = (rowAddr + c) & 0xffff;
        let cls = "hex-byte";
        if (old && old[i] !== v) cls += " changed";
        if (a === 0x100 + sp) cls += " is-sp";
        if (c === 8) hexPart += `<span class="gap"></span>`;
        hexPart += `<span class="${cls}" data-addr="${a}">${hex2(v)}</span>`;
        const ch = v & 0x7f;
        ascii += ch >= 0x20 && ch < 0x7f ? String.fromCharCode(ch).replace("&", "&amp;").replace("<", "&lt;") : ".";
      }
      html += `<div class="hex-row"><span class="hex-addr">${hex4(rowAddr)}</span>` +
              `<span class="hex-bytes">${hexPart}</span><span class="hex-ascii">${ascii}</span></div>`;
    }
    $("#mem").innerHTML = html;
    prev.mem = bytes;
    prev.memAddr = memAddr;
  }

  function setLeds(leds, pins, ddr) {
    leds.forEach((led, i) => {
      const mask = 0x80 >> i;
      led.classList.toggle("is-input", !(ddr & mask));
      led.classList.toggle("is-on", !!(pins & mask));
    });
  }

  function kv(el, pairs) {
    el.innerHTML = pairs.map(([k, v]) => `<dt>${k}</dt><dd>${v}</dd>`).join("");
  }

  function updateIO() {
    setLeds(ledsA, E.emu_dev(DEV.PORTA), E.emu_dev(DEV.DDRA));
    setLeds(ledsB, E.emu_dev(DEV.PORTB), E.emu_dev(DEV.DDRB));
    kv($("#via"), [
      ["DDRA", hex2(E.emu_dev(DEV.DDRA))], ["DDRB", hex2(E.emu_dev(DEV.DDRB))],
      ["Timer 1", hex4(E.emu_dev(DEV.T1))], ["Timer 2", hex4(E.emu_dev(DEV.T2))],
      ["ACR", hex2(E.emu_dev(DEV.ACR))], ["IFR", hex2(E.emu_dev(DEV.IFR))],
    ]);
    const st = E.emu_dev(DEV.ACIA_STATUS);
    kv($("#acia"), [
      ["ACIA status", hex2(st)], ["Key waiting", st & 0x08 ? "yes" : "no"],
      ["Command", hex2(E.emu_dev(DEV.ACIA_COMMAND))], ["Control", hex2(E.emu_dev(DEV.ACIA_CONTROL))],
    ]);
    const q = E.emu_key_count();
    $("#keyq").textContent = q ? `${q} key${q === 1 ? "" : "s"} waiting for the 6502` : "";
  }

  function updatePanels() {
    if (!state.hasRom) return;
    const pc = E.emu_reg(REG.PC);
    const sp = E.emu_reg(REG.SP);
    const p = E.emu_reg(REG.P);

    setReg("#r-pc", "pc", hex4(pc));
    setReg("#r-a", "a", hex2(E.emu_reg(REG.A)));
    setReg("#r-x", "x", hex2(E.emu_reg(REG.X)));
    setReg("#r-y", "y", hex2(E.emu_reg(REG.Y)));
    setReg("#r-sp", "sp", hex2(sp));
    setReg("#r-p", "p", hex2(p));
    flagEls.forEach((el, i) => el.classList.toggle("is-set", !!(p & (0x80 >> i))));

    // Cycles and the measured speed.
    const now = performance.now();
    const cycles = E.emu_cycles();
    if (now - state.mhzSample.t > 500) {
      const dt = (now - state.mhzSample.t) / 1000;
      state.mhz = state.mhzSample.t ? (cycles - state.mhzSample.cycles) / dt / 1e6 : 0;
      state.mhzSample = { t: now, cycles };
    }
    const speed = state.running ? `${state.mhz.toFixed(state.mhz < 10 ? 2 : 1)} MHz, ` : "";
    $("#cycles").textContent = `${speed}${Math.round(cycles).toLocaleString("en-US")} cycles`;

    updateDisassembly(pc);
    updateStack(sp);
    updateMemory(sp);
    updateIO();

    const status = $("#status");
    status.textContent = state.message;
    status.className = "status" + (state.messageKind ? ` is-${state.messageKind}` : "");
  }

  function updateButtons() {
    const run = $("#btn-run");
    run.textContent = state.running ? "Pause" : "Run";
    run.classList.toggle("is-running", state.running);
    for (const id of ["#btn-run", "#btn-step", "#btn-over", "#btn-reset", "#btn-power", "#btn-prog"]) {
      $(id).disabled = !state.hasRom;
    }
  }

  /* Memory viewer controls */

  function goToMemory(addr) {
    memAddr = addr & 0xfff0;
    $("#mem-addr").value = hex4(memAddr);
    updatePanels();
  }

  $("#mem-addr").addEventListener("keydown", (e) => {
    if (e.key !== "Enter") return;
    const addr = parseInt(e.target.value, 16);
    if (!Number.isNaN(addr)) goToMemory(addr);
  });
  document.querySelectorAll(".presets .chip").forEach((b) =>
    b.addEventListener("click", () => goToMemory(parseInt(b.dataset.addr, 16))));

  $("#mem").addEventListener("click", (e) => {
    const el = e.target.closest(".hex-byte");
    if (!el) return;
    const addr = Number(el.dataset.addr);
    const answer = window.prompt(`New value for $${hex4(addr)} (hex)`, el.textContent);
    if (answer === null) return;
    const v = parseInt(answer, 16);
    if (!Number.isNaN(v) && v >= 0 && v <= 0xff) {
      E.emu_load_byte(addr, v);
      updatePanels();
    }
  });

  $("#mem").addEventListener("wheel", (e) => {
    if (e.deltaY === 0) return;
    e.preventDefault();
    goToMemory((memAddr + (e.deltaY > 0 ? 0x10 : -0x10)) & 0xffff);
  }, { passive: false });

  /* ------------------------------------------------------------------ */
  /* Files                                                               */
  /* ------------------------------------------------------------------ */

  const ROM_KEY = "emu6502.rom";

  function loadRom(bytes, remember = true) {
    const size = E.emu_rom_size();
    if (bytes.length === 0 || bytes.length > size) {
      setStatus(`A ROM image must be 1 to ${size} bytes; this file has ${bytes.length}.`, "error");
      updatePanels();
      return false;
    }
    state.running = false;
    clearTempBreak();
    E.emu_init();
    mem().set(bytes, E.emu_rom_ptr() + size - bytes.length);   // ends at $FFFF
    for (const a of breakpoints) bpTable()[a] = 1;
    E.emu_reset();
    term.clear();
    term.clearHistory();
    state.hasRom = true;
    $("#term-empty").hidden = true;
    if (remember) {
      try {
        let s = "";
        for (const b of bytes) s += String.fromCharCode(b);
        localStorage.setItem(ROM_KEY, btoa(s));
      } catch (_) { /* storage unavailable: just don't remember it */ }
    }
    setRunning(true);
    termEl.focus();
    return true;
  }

  function parseWozHex(text) {
    // Lines like "0300: A9 01 85 00". Returns [address, value] pairs.
    const out = [];
    let lineNo = 0;
    for (const raw of text.split(/\r?\n/)) {
      lineNo++;
      const line = raw.replace(/[;#].*$/, "").trim();
      if (!line) continue;
      const m = /^([0-9A-Fa-f]{1,4})\s*:\s*(.*)$/.exec(line);
      if (!m) throw new Error(`line ${lineNo} is not "ADDR: bytes"`);
      let addr = parseInt(m[1], 16);
      for (const tok of m[2].split(/\s+/).filter(Boolean)) {
        const v = parseInt(tok, 16);
        if (!/^[0-9A-Fa-f]{1,2}$/.test(tok)) throw new Error(`line ${lineNo}: "${tok}" is not a byte`);
        out.push([addr & 0xffff, v]);
        addr++;
      }
    }
    return out;
  }

  function loadProgram(name, bytes) {
    try {
      let pairs;
      if (/\.(hex|txt)$/i.test(name)) {
        pairs = parseWozHex(new TextDecoder().decode(bytes));
      } else {
        const start = parseInt($("#prog-addr").value, 16);
        if (Number.isNaN(start)) throw new Error("the load address is not a hex number");
        pairs = Array.from(bytes, (v, i) => [(start + i) & 0xffff, v]);
      }
      if (pairs.length === 0) throw new Error("the file is empty");
      for (const [a, v] of pairs) E.emu_load_byte(a, v);
      const first = pairs[0][0];
      setStatus(`Loaded ${name}: ${pairs.length} bytes at $${hex4(first)}. Type ${hex4(first)}R in WozMon to run it.`);
      goToMemory(first);
    } catch (err) {
      setStatus(`${name}: ${err.message}`, "error");
      updatePanels();
    }
  }

  async function readFile(file) {
    return new Uint8Array(await file.arrayBuffer());
  }

  $("#btn-rom").addEventListener("click", () => $("#file-rom").click());
  $("#btn-rom-2").addEventListener("click", () => $("#file-rom").click());
  $("#btn-prog").addEventListener("click", () => $("#file-prog").click());
  $("#file-rom").addEventListener("change", async (e) => {
    const f = e.target.files[0];
    if (f) loadRom(await readFile(f));
    e.target.value = "";
  });
  $("#file-prog").addEventListener("change", async (e) => {
    const f = e.target.files[0];
    if (f) loadProgram(f.name, await readFile(f));
    e.target.value = "";
  });

  // Drag and drop anywhere: a 32 KiB file or one called rom* is a ROM.
  document.addEventListener("dragover", (e) => {
    e.preventDefault();
    document.body.classList.add("is-dragging");
  });
  document.addEventListener("dragleave", (e) => {
    if (e.target === document.documentElement || e.clientX === 0) document.body.classList.remove("is-dragging");
  });
  document.addEventListener("drop", async (e) => {
    e.preventDefault();
    document.body.classList.remove("is-dragging");
    const f = e.dataTransfer.files[0];
    if (!f) return;
    const bytes = await readFile(f);
    if (bytes.length === E.emu_rom_size() || /^rom/i.test(f.name) || !state.hasRom) loadRom(bytes);
    else loadProgram(f.name, bytes);
  });

  /* ------------------------------------------------------------------ */
  /* Buttons and shortcuts                                               */
  /* ------------------------------------------------------------------ */

  $("#btn-run").addEventListener("click", () => setRunning(!state.running));
  $("#btn-step").addEventListener("click", step);
  $("#btn-over").addEventListener("click", stepOver);
  $("#btn-reset").addEventListener("click", () => {
    clearTempBreak();
    E.emu_reset();
    setStatus("Reset");
    setRunning(true);
    termEl.focus();
  });
  $("#btn-power").addEventListener("click", () => {
    let saved = null;
    try { saved = localStorage.getItem(ROM_KEY); } catch (_) { /* ignore */ }
    if (saved) loadRom(Uint8Array.from(atob(saved), (c) => c.charCodeAt(0)), false);
  });
  $("#speed").addEventListener("change", (e) => {
    state.speed = Number(e.target.value);
    state.cycleCarry = 0;
  });
  $("#prog-addr").addEventListener("input", (e) => {
    e.target.value = e.target.value.replace(/[^0-9a-f]/gi, "").toUpperCase();
  });

  document.addEventListener("keydown", (e) => {
    if (e.key === "F9") { e.preventDefault(); setRunning(!state.running); }
    else if (e.key === "F7") { e.preventDefault(); step(); }
    else if (e.key === "F8") { e.preventDefault(); stepOver(); }
  });

  /* ------------------------------------------------------------------ */
  /* Start                                                               */
  /* ------------------------------------------------------------------ */

  E.emu_init();
  updateButtons();
  term.render();

  let started = false;
  try {
    // When the page is served over http (GitHub Pages, a local server),
    // a rom.bin next to it is loaded first.
    const res = await fetch("rom.bin", { cache: "no-cache" });
    if (res.ok) started = loadRom(new Uint8Array(await res.arrayBuffer()), false);
  } catch (_) { /* opened from disk, or no rom.bin */ }

  if (!started) {
    // Otherwise, the ROM loaded by hand last time.
    try {
      const saved = localStorage.getItem(ROM_KEY);
      if (saved) started = loadRom(Uint8Array.from(atob(saved), (c) => c.charCodeAt(0)), false);
    } catch (_) { /* no storage */ }
  }

  if (!started) {
    $("#term-empty").hidden = false;
    setStatus("No ROM loaded");
    $("#status").textContent = state.message;
  }

  requestAnimationFrame(frame);
})();
