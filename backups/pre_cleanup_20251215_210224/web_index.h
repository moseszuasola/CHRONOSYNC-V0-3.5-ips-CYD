const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ChronoSync | Your Digital Memory Vault</title>
    <!-- Tailwind CSS -->
    <script src="https://cdn.tailwindcss.com"></script>
    <!-- Phosphor Icons -->
    <script src="https://unpkg.com/@phosphor-icons/web"></script>
    <!-- SortableJS -->
    <script src="https://cdnjs.cloudflare.com/ajax/libs/Sortable/1.15.0/Sortable.min.js"></script>
    <!-- Cropper.js -->
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/cropperjs/1.5.13/cropper.min.css">
    <script src="https://cdnjs.cloudflare.com/ajax/libs/cropperjs/1.5.13/cropper.min.js"></script>
    
    <!-- EMBEDDED GIF DECODER -->
    <script>
    (function(window) {
        function lzwDecode(minCodeSize, data) {
            var pos = 0, size = 0, val = 0;
            var MAX_CODE_SIZE = 12;
            var clearCode = 1 << minCodeSize;
            var eoiCode = clearCode + 1;
            var codeSize = minCodeSize + 1;
            var dict = [];
            var output = [];
            function initDict() {
                dict = [];
                for (var i = 0; i < clearCode; i++) dict[i] = [i];
                dict[clearCode] = [];
                dict[eoiCode] = null;
            }
            function read() {
                while (size < codeSize) {
                    if (pos >= data.length) return eoiCode;
                    val |= data[pos++] << size;
                    size += 8;
                }
                var code = val & ((1 << codeSize) - 1);
                val >>= codeSize;
                size -= codeSize;
                return code;
            }
            initDict();
            var code, last;
            while (true) {
                last = code;
                code = read();
                if (code === clearCode) { initDict(); codeSize = minCodeSize + 1; code = read(); if (code === eoiCode) break; output.push.apply(output, dict[code]); continue; }
                if (code === eoiCode) break;
                if (code < dict.length) {
                    if (last !== clearCode) { dict.push(dict[last].concat(dict[code][0])); }
                    output.push.apply(output, dict[code]);
                } else {
                    if (last === clearCode) throw new Error("Invalid LZW");
                    var entry = dict[last].concat(dict[last][0]);
                    output.push.apply(output, entry);
                    dict.push(entry);
                }
                if (dict.length === (1 << codeSize) && codeSize < MAX_CODE_SIZE) codeSize++;
            }
            return output;
        }
        function deinterlace(pixels, width) {
            var newPixels = new Array(pixels.length);
            var rows = pixels.length / width;
            var offsets = [0, 4, 2, 1], steps = [8, 8, 4, 2];
            var fromRow = 0;
            for (var pass = 0; pass < 4; pass++) {
                for (var toRow = offsets[pass]; toRow < rows; toRow += steps[pass]) {
                    var start = fromRow * width, end = start + width;
                    for (var x = 0; x < width; x++) newPixels[toRow * width + x] = pixels[start + x];
                    fromRow++;
                }
            }
            return newPixels;
        }
        window.parseGIF = function(arrayBuffer) {
            var data = new Uint8Array(arrayBuffer);
            var p = 0;
            function readByte() { return data[p++]; }
            function readWord() { return data[p++] | (data[p++] << 8); }
            function readBytes(n) { var arr = data.subarray(p, p + n); p += n; return arr; }
            function readString(n) { var s = ''; for (var i = 0; i < n; i++) s += String.fromCharCode(readByte()); return s; }
            var sig = readString(3);
            var ver = readString(3);
            if (sig !== 'GIF') throw new Error('Not a GIF file');
            var width = readWord();
            var height = readWord();
            var packed = readByte();
            var bgIndex = readByte();
            var aspect = readByte();
            var gctFlag = (packed >> 7) & 1;
            var gctSize = packed & 7;
            var gct = null;
            if (gctFlag) {
                var gctLen = 1 << (gctSize + 1);
                gct = [];
                for (var i = 0; i < gctLen; i++) gct.push([readByte(), readByte(), readByte()]);
            }
            var frames = [];
            var gce = null;
            while (p < data.length) {
                var block = readByte();
                if (block === 0x21) { 
                    var extType = readByte();
                    if (extType === 0xF9) { 
                        readByte(); 
                        var gcePacked = readByte();
                        var delay = readWord();
                        var transIndex = readByte();
                        readByte(); 
                        gce = { disposal: (gcePacked >> 2) & 7, transparent: gcePacked & 1, delay: delay * 10, transIndex: transIndex };
                    } else { 
                        var size = readByte();
                        while (size > 0) { p += size; size = readByte(); }
                    }
                } else if (block === 0x2C) { 
                    var imgLeft = readWord();
                    var imgTop = readWord();
                    var imgWidth = readWord();
                    var imgHeight = readWord();
                    var imgPacked = readByte();
                    var lctFlag = (imgPacked >> 7) & 1;
                    var interlaced = (imgPacked >> 6) & 1;
                    var lctSize = imgPacked & 7;
                    var lct = null;
                    if (lctFlag) {
                        var lctLen = 1 << (lctSize + 1);
                        lct = [];
                        for (var j = 0; j < lctLen; j++) lct.push([readByte(), readByte(), readByte()]);
                    }
                    var minCodeSize = readByte();
                    var imgData = [];
                    var subSize = readByte();
                    while (subSize > 0) {
                        for (var k = 0; k < subSize; k++) imgData.push(readByte());
                        subSize = readByte();
                    }
                    var pixels = lzwDecode(minCodeSize, imgData);
                    if (interlaced) pixels = deinterlace(pixels, imgWidth);
                    var colorTable = lct || gct;
                    var frame = {
                        dims: { left: imgLeft, top: imgTop, width: imgWidth, height: imgHeight },
                        colorTable: colorTable,
                        pixels: pixels,
                        delay: gce ? gce.delay : 100,
                        disposal: gce ? gce.disposal : 0,
                        transparent: gce ? gce.transparent : false,
                        transIndex: gce ? gce.transIndex : 0
                    };
                    var patch = new Uint8ClampedArray(imgWidth * imgHeight * 4);
                    for (var pi = 0; pi < pixels.length; pi++) {
                        var colorIdx = pixels[pi];
                        var rgba = pi * 4;
                        if (frame.transparent && colorIdx === frame.transIndex) {
                            patch[rgba] = 0; patch[rgba+1] = 0; patch[rgba+2] = 0; patch[rgba+3] = 0;
                        } else {
                            var c = colorTable[colorIdx] || [0,0,0];
                            patch[rgba] = c[0]; patch[rgba+1] = c[1]; patch[rgba+2] = c[2]; patch[rgba+3] = 255;
                        }
                    }
                    frame.patch = patch;
                    frames.push(frame);
                    gce = null;
                } else if (block === 0x3B) { break;
                } else if (block === 0x00) { continue;
                } else { break; }
            }
            return { width: width, height: height, frames: frames };
        };
    })(window);
    </script>

    <style>
        ::-webkit-scrollbar { width: 6px; } ::-webkit-scrollbar-track { background: #0f172a; } ::-webkit-scrollbar-thumb { background: #334155; border-radius: 3px; }
        .glass-panel { background: rgba(30, 41, 59, 0.8); backdrop-filter: blur(12px); border: 1px solid rgba(255, 255, 255, 0.08); }
        .tab-active { border-bottom: 2px solid #06b6d4; color: #06b6d4; background: rgba(6, 182, 212, 0.05); }
        .tab-inactive { color: #64748b; border-bottom: 2px solid transparent; }
        .sortable-ghost { opacity: 0.3; background: #06b6d4; } 
        .sortable-drag { opacity: 1.0; cursor: grabbing; transform: scale(1.05); box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.5); z-index: 50; } 
        .sortable-chosen { border: 2px solid #06b6d4; }
        .no-context-menu { -webkit-touch-callout: none !important; -webkit-user-select: none !important; user-select: none !important; }
        .cropper-view-box, .cropper-face { border-radius: 0; }
        .cropper-modal { background-color: #0f172a; opacity: 0.85; }
        .cropper-bg { background-image: none; background-color: #0f172a; }
        .cropper-point { width: 10px !important; height: 10px !important; background-color: #06b6d4 !important; opacity: 0.9 !important; }
        .cropper-line { background-color: #06b6d4 !important; opacity: 0.5 !important; }
    </style>
</head>
<body class="bg-slate-950 text-slate-200 min-h-screen font-sans pb-32 selection:bg-cyan-500 selection:text-white relative">

    <div id="busy-overlay" class="fixed inset-0 z-[100] bg-slate-950/90 backdrop-blur-sm hidden flex-col items-center justify-center pointer-events-auto transition-all duration-300">
        <div class="w-12 h-12 border-4 border-slate-700 border-t-cyan-500 rounded-full animate-spin mb-4"></div>
        <span id="busy-text" class="text-cyan-400 font-bold tracking-widest text-sm animate-pulse">CONNECTING...</span>
    </div>

    <nav class="sticky top-0 z-40 glass-panel border-b border-slate-800 px-4 py-3 flex justify-center items-center shadow-lg">
        <div class="flex items-center gap-2">
            <div class="w-2.5 h-2.5 rounded-full bg-cyan-400 animate-pulse shadow-[0_0_10px_#22d3ee]"></div>
            <h1 class="font-bold tracking-wider text-cyan-400 text-lg">CHRONOSYNC</h1>
        </div>
    </nav>

    <!-- RESPONSIVE CONTAINER: Mobile: max-w-md, Tablet: max-w-3xl, PC: max-w-[55rem] -->
    <main class="w-full max-w-md md:max-w-3xl lg:max-w-[55rem] mx-auto p-4 space-y-6 transition-all duration-300">
        
        <div class="glass-panel rounded-2xl p-5 shadow-xl">
            <div class="flex justify-between items-start mb-4">
                <div>
                    <h2 class="text-[10px] uppercase tracking-widest text-slate-500 font-bold mb-1">System Status</h2>
                    <div id="clock-display" class="text-3xl font-bold text-white tracking-tight">--:--</div>
                    <div id="date-display" class="text-xs text-cyan-400/80 font-medium mt-0.5">Syncing...</div>
                </div>
                <div class="text-right">
                    <div class="text-[10px] text-slate-500 font-bold uppercase">Device IP</div>
                    <div class="font-mono text-cyan-400 text-sm bg-cyan-950/30 px-2 py-1 rounded border border-cyan-900/50 mt-1" id="ip-addr">...</div>
                </div>
            </div>
            <div class="grid grid-cols-2 gap-2">
                <button id="btn-orientation" onclick="toggleOrientation()" class="bg-slate-800 hover:bg-slate-700 text-slate-200 py-2.5 rounded-xl text-xs font-semibold flex items-center justify-center gap-2 transition-all active:scale-95 border border-slate-700">
                    <i class="ph-bold ph-device-rotate text-lg"></i> <span id="orientation-text">Portrait</span>
                </button>
                <button onclick="document.getElementById('wifi-modal').classList.remove('hidden')" class="bg-slate-800 hover:bg-slate-700 text-slate-200 py-2.5 rounded-xl text-xs font-semibold flex items-center justify-center gap-2 transition-all active:scale-95 border border-slate-700">
                    <i class="ph-bold ph-wifi-high text-lg"></i> WiFi
                </button>
            </div>
            <div class="grid grid-cols-2 gap-2 mt-2">
                <button onclick="openClockSettings()" class="bg-slate-800 hover:bg-slate-700 text-slate-200 py-2.5 rounded-xl text-xs font-semibold flex items-center justify-center gap-2 transition-all active:scale-95 border border-slate-700">
                    <i class="ph-bold ph-gear text-lg"></i> Clock Settings
                </button>
                <button onclick="document.getElementById('devices-modal').classList.remove('hidden')" class="bg-gradient-to-r from-cyan-900/30 to-blue-900/30 hover:from-cyan-900/50 hover:to-blue-900/50 text-cyan-400 py-2.5 rounded-xl text-xs font-semibold flex items-center justify-center gap-2 transition-all active:scale-95 border border-cyan-800/50">
                    <i class="ph-bold ph-devices text-lg"></i> Devices
                </button>
            </div>
        </div>

        <div class="grid grid-cols-2 border-b border-slate-800">
            <button id="tab-photos" onclick="switchTab('photos')" class="tab-active text-sm font-bold py-3 transition-all flex items-center justify-center gap-2 rounded-t-xl">
                <i class="ph-fill ph-image"></i> Photos
            </button>
            <button id="tab-gifs" onclick="switchTab('gifs')" class="tab-inactive text-sm font-bold py-3 transition-all flex items-center justify-center gap-2 rounded-t-xl hover:text-slate-300">
                <i class="ph-fill ph-film-strip"></i> GIFs
            </button>
        </div>

        <div class="glass-panel rounded-b-2xl rounded-tr-2xl p-5 transition-all duration-300 mt-[-1px]">
            
            <div id="controls-photos" class="space-y-4">
                <div class="grid grid-cols-2 gap-3">
                    <button id="btn-slideshow" onclick="toggleSlideshow()" disabled title="Play a photo first to enable slideshow" class="bg-slate-800/50 border border-slate-700 text-slate-500 py-3.5 rounded-xl flex flex-col items-center gap-1 transition-all cursor-not-allowed group">
                        <i class="ph-fill ph-play-circle text-2xl text-slate-500 transition-colors" id="icon-slideshow"></i>
                        <span class="text-xs font-bold" id="text-slideshow">Start Slideshow</span>
                        <span class="text-[10px] font-medium text-slate-400 mt-0.5" id="slideshow-hint">Play a photo first</span>
                    </button>
                    <button onclick="document.getElementById('fileInput').click()" class="bg-slate-800 hover:bg-slate-700 border border-slate-700 text-white py-3.5 rounded-xl flex flex-col items-center gap-1 transition-all active:scale-95 group">
                        <i class="ph-bold ph-upload-simple text-2xl text-cyan-400 group-hover:text-cyan-300 transition-colors"></i>
                        <span class="text-xs font-bold">Upload Photo</span>
                    </button>
                </div>
                <div class="bg-slate-900/50 rounded-xl p-3 border border-slate-800 flex items-center gap-3">
                    <div class="bg-slate-800 p-2 rounded-lg text-slate-400"><i class="ph-fill ph-timer text-lg"></i></div>
                    <div class="flex-1">
                        <label class="text-[10px] text-slate-400 uppercase font-bold">Slide Duration</label>
                        <div class="flex gap-2 mt-0.5 items-baseline">
                            <input type="number" id="dur-input" value="10" min="3" class="bg-transparent border-b border-slate-600 text-white w-12 text-sm font-mono focus:border-cyan-500 outline-none text-center p-0">
                            <span class="text-xs text-slate-500 font-medium">seconds</span>
                        </div>
                    </div>
                    <button onclick="saveDur()" class="bg-slate-700 hover:bg-slate-600 text-white px-4 py-1.5 rounded-lg text-xs font-bold transition-colors">Save</button>
                </div>
            </div>

            <div id="controls-gifs" class="hidden space-y-4">
                 <button onclick="document.getElementById('fileInput').click()" class="w-full bg-slate-800 hover:bg-slate-700 border border-slate-700 text-white py-6 rounded-xl flex items-center justify-center gap-3 transition-all active:scale-95 group shadow-lg">
                    <div class="bg-cyan-900/30 p-2 rounded-full group-hover:scale-110 transition-transform">
                        <i class="ph-bold ph-upload-simple text-2xl text-cyan-400 group-hover:text-cyan-300"></i>
                    </div>
                    <div class="text-left">
                        <div class="text-sm font-bold tracking-wide">Upload New GIF</div>
                        <div class="text-[10px] text-slate-400 font-medium">Supports animations</div>
                    </div>
                </button>
            </div>

        </div>

        <div>
            <!-- Header with Save/Cancel controls -->
            <div class="flex justify-between items-end mb-3 px-1">
                <div>
                     <h3 class="text-xs font-bold text-slate-400 uppercase tracking-widest mb-1">Library Content</h3>
                     <p class="text-[10px] text-cyan-500 font-bold" id="current-view-label">Landscape Gallery</p>
                </div>
                
                <div class="flex gap-2 items-center" id="library-actions">
                    <!-- Default: Refresh -->
                    <button id="btn-refresh" onclick="loadGallery(true)" class="text-xs bg-slate-800 text-cyan-400 px-3 py-1.5 rounded-lg font-bold border border-slate-700 hover:bg-slate-700 transition-colors flex items-center gap-1">
                        <i class="ph-bold ph-arrows-clockwise"></i> Refresh
                    </button>
                    
                    <!-- Edit Mode: Save/Cancel (Hidden by default) -->
                    <div id="btn-actions" class="hidden flex gap-2">
                        <button onclick="cancelReorder()" class="text-xs bg-slate-800 text-slate-300 px-3 py-1.5 rounded-lg font-bold border border-slate-700 hover:bg-slate-700 transition-colors">Cancel</button>
                        <button onclick="saveOrder()" class="text-xs bg-cyan-600 text-white px-3 py-1.5 rounded-lg font-bold shadow-lg hover:bg-cyan-500 transition-colors">Save</button>
                    </div>
                </div>
            </div>
            
            <div id="gallery-grid" class="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-4 pb-4"></div>
            
            <div id="empty-state" class="hidden flex flex-col items-center justify-center py-16 text-slate-600 gap-3 border-2 border-dashed border-slate-800 rounded-2xl bg-slate-900/30">
                <i class="ph-duotone ph-images text-4xl opacity-50"></i>
                <span class="text-xs font-medium">No media found.</span>
            </div>
        </div>
    </main>

    <input type="file" id="fileInput" accept="image/jpeg,image/png,image/jpg" class="hidden" onchange="handleFileSelect()">

    <!-- MODALS -->
    <div id="wifi-modal" class="fixed inset-0 z-50 bg-slate-950/90 hidden flex items-center justify-center p-6 backdrop-blur-md">
        <div class="glass-panel w-full max-w-sm rounded-2xl p-6 border-slate-700 shadow-2xl">
            <h3 class="text-lg font-bold text-white mb-6 flex items-center gap-2"><i class="ph-fill ph-wifi-high text-cyan-400"></i> Network Setup</h3>
            <div class="space-y-4">
                <div>
                    <label class="text-xs text-slate-400 font-bold ml-1 mb-1 block">SSID (Network Name)</label>
                    <input type="text" id="wifiSsid" class="w-full bg-slate-900 border border-slate-700 rounded-xl p-3 text-white focus:border-cyan-500 outline-none transition-colors">
                </div>
                <div>
                    <label class="text-xs text-slate-400 font-bold ml-1 mb-1 block">Password</label>
                    <input type="password" id="wifiPass" class="w-full bg-slate-900 border border-slate-700 rounded-xl p-3 text-white focus:border-cyan-500 outline-none transition-colors">
                </div>
            </div>
            <div class="flex gap-3 mt-8">
                <button onclick="document.getElementById('wifi-modal').classList.add('hidden')" class="flex-1 bg-slate-800 hover:bg-slate-700 text-slate-300 py-3 rounded-xl font-bold transition-colors">Cancel</button>
                <button onclick="updateWiFi()" class="flex-1 bg-gradient-to-r from-cyan-600 to-blue-600 hover:from-cyan-500 hover:to-blue-500 text-white py-3 rounded-xl font-bold shadow-lg transition-all">Connect</button>
            </div>
        </div>
    </div>

    <div id="clock-settings-modal" class="fixed inset-0 z-50 bg-slate-950/90 hidden flex items-center justify-center p-6 backdrop-blur-md">
        <div class="glass-panel w-full max-w-sm rounded-2xl p-6 border-slate-700 shadow-2xl max-h-[90vh] overflow-y-auto">
            <h3 class="text-lg font-bold text-white mb-6 flex items-center gap-2"><i class="ph-fill ph-clock text-cyan-400"></i> Clock Settings</h3>
            <div class="space-y-4">
                <div class="flex items-center justify-between bg-slate-900 border border-slate-700 rounded-xl p-4">
                    <div>
                        <label class="text-sm text-slate-200 font-semibold">Enable Clock</label>
                        <p class="text-[10px] text-slate-500 mt-1">Show clock during slideshow</p>
                    </div>
                    <label class="relative inline-block w-12 h-6 cursor-pointer">
                        <input id="clockEnabled" type="checkbox" checked class="sr-only peer">
                        <div class="w-full h-full bg-slate-700 rounded-full peer-checked:bg-cyan-600 transition-colors"></div>
                        <div class="absolute top-0.5 left-0.5 w-5 h-5 bg-white rounded-full transition-transform peer-checked:translate-x-6"></div>
                    </label>
                </div>
                <div class="flex items-center justify-between bg-slate-900 border border-slate-700 rounded-xl p-4">
                    <div>
                        <label class="text-sm text-slate-200 font-semibold">Show at End of Slideshow</label>
                        <p class="text-[10px] text-slate-500 mt-1">Clock appears after all photos shown</p>
                    </div>
                    <label class="relative inline-block w-12 h-6 cursor-pointer">
                        <input id="clockShowAtEnd" type="checkbox" checked class="sr-only peer">
                        <div class="w-full h-full bg-slate-700 rounded-full peer-checked:bg-cyan-600 transition-colors"></div>
                        <div class="absolute top-0.5 left-0.5 w-5 h-5 bg-white rounded-full transition-transform peer-checked:translate-x-6"></div>
                    </label>
                </div>
                <div>
                    <label class="text-xs text-slate-400 font-bold ml-1 mb-2 block">Time Format</label>
                    <div class="flex bg-slate-900 rounded-lg p-1 border border-slate-700">
                        <button id="btn-12hr" onclick="setClockFormat(false)" class="flex-1 px-4 py-2.5 rounded-md text-sm font-bold transition-all bg-slate-700 text-white">12 Hour</button>
                        <button id="btn-24hr" onclick="setClockFormat(true)" class="flex-1 px-4 py-2.5 rounded-md text-sm font-bold transition-all text-slate-400">24 Hour</button>
                    </div>
                </div>
                <div>
                    <label class="text-xs text-slate-400 font-bold ml-1 mb-1 block">Timezone</label>
                    <select id="clockTimezone" class="w-full bg-slate-900 border border-slate-700 rounded-xl p-3 text-white focus:border-cyan-500 outline-none transition-colors">
                        <option value="-43200">UTC-12 Baker Island</option>
                        <option value="-39600">UTC-11 American Samoa</option>
                        <option value="-36000">UTC-10 Hawaii</option>
                        <option value="-32400">UTC-9 Alaska</option>
                        <option value="-28800">UTC-8 Pacific Time</option>
                        <option value="-25200">UTC-7 Mountain Time</option>
                        <option value="-21600">UTC-6 Central Time</option>
                        <option value="-18000">UTC-5 Eastern Time</option>
                        <option value="-14400">UTC-4 Atlantic Time</option>
                        <option value="-10800">UTC-3 Buenos Aires</option>
                        <option value="-7200">UTC-2 South Georgia</option>
                        <option value="-3600">UTC-1 Azores</option>
                        <option value="0">UTC+0 London</option>
                        <option value="3600">UTC+1 Paris</option>
                        <option value="7200">UTC+2 Cairo</option>
                        <option value="10800">UTC+3 Moscow</option>
                        <option value="14400">UTC+4 Dubai</option>
                        <option value="18000">UTC+5 Karachi</option>
                        <option value="19800">UTC+5:30 Mumbai</option>
                        <option value="21600">UTC+6 Dhaka</option>
                        <option value="25200">UTC+7 Bangkok</option>
                        <option value="28800" selected>UTC+8 Manila</option>
                        <option value="32400">UTC+9 Tokyo</option>
                        <option value="36000">UTC+10 Sydney</option>
                        <option value="39600">UTC+11 Solomon Islands</option>
                        <option value="43200">UTC+12 Fiji</option>
                        <option value="46800">UTC+13 Tonga</option>
                        <option value="50400">UTC+14 Kiribati</option>
                    </select>
                </div>
                <div>
                    <label class="text-xs text-slate-400 font-bold ml-1 mb-1 block">Display Duration (seconds)</label>
                    <input id="clockDisplayDuration" type="number" min="5" max="300" value="30" class="w-full bg-slate-900 border border-slate-700 rounded-xl p-3 text-white focus:border-cyan-500 outline-none transition-colors" placeholder="30">
                    <span class="text-[10px] text-slate-500 ml-1 mt-1 block">How long clock shows before resuming slideshow</span>
                </div>
                <div id="photoIntervalSection">
                    <label class="text-xs text-slate-400 font-bold ml-1 mb-1 block">Photo Interval</label>
                    <input id="clockPhotoInterval" type="number" min="1" max="20" value="3" class="w-full bg-slate-900 border border-slate-700 rounded-xl p-3 text-white focus:border-cyan-500 outline-none transition-colors" placeholder="3">
                    <span class="text-[10px] text-slate-500 ml-1 mt-1 block">Show clock every N photos (when "Show at End" is off)</span>
                </div>
                <div class="flex items-center justify-between bg-slate-900 border border-slate-700 rounded-xl p-4">
                    <div>
                        <label class="text-sm text-slate-200 font-semibold">Clock-Only Mode</label>
                        <p class="text-[10px] text-slate-500 mt-1">Display clock permanently (no slideshow)</p>
                    </div>
                    <label class="relative inline-block w-12 h-6 cursor-pointer">
                        <input id="clockOnlyScreen" type="checkbox" class="sr-only peer">
                        <div class="w-full h-full bg-slate-700 rounded-full peer-checked:bg-cyan-600 transition-colors"></div>
                        <div class="absolute top-0.5 left-0.5 w-5 h-5 bg-white rounded-full transition-transform peer-checked:translate-x-6"></div>
                    </label>
                </div>

                <div class="bg-slate-900/50 rounded-xl p-4 border border-slate-800">
                    <h4 class="text-sm font-bold text-slate-200 mb-3">Slideshow Info Bar</h4>
                    <div class="space-y-3">
                        <div>
                            <label class="text-xs text-slate-400 font-bold ml-1 mb-2 block">Position</label>
                            <div class="flex bg-slate-900 rounded-lg p-1 border border-slate-700">
                                <button id="btn-bar-top" onclick="setBarPosition('top')" class="flex-1 px-4 py-2.5 rounded-md text-sm font-bold transition-all bg-slate-700 text-white">Top</button>
                                <button id="btn-bar-bottom" onclick="setBarPosition('bottom')" class="flex-1 px-4 py-2.5 rounded-md text-sm font-bold transition-all text-slate-400">Bottom</button>
                            </div>
                        </div>
                        <div>
                            <label class="text-xs text-slate-400 font-bold ml-1 mb-1 block">Custom Messages (max 20 chars per line)</label>
                            <textarea id="customMessages" rows="3" maxlength="200" class="w-full bg-slate-900 border border-slate-700 rounded-xl p-3 text-white focus:border-cyan-500 outline-none transition-colors text-sm" placeholder="Creating memories...
Life is beautiful!
Enjoy the moments!" oninput="validateMessageLength(this)">Creating memories...
Life is beautiful!
Enjoy the moments!
You're amazing!
Never give up!</textarea>
                            <div class="text-[10px] text-slate-500 mt-1">Each line limited to 20 characters for proper display</div>
                            <div id="messageValidation" class="text-[10px] text-red-400 mt-1 hidden">Some lines exceed 20 characters and will be truncated</div>
                        </div>
                    </div>
                </div>
                <button onclick="syncTime()" class="w-full bg-slate-800 hover:bg-slate-700 text-slate-200 py-3 rounded-xl text-sm font-semibold flex items-center justify-center gap-2 transition-all active:scale-95 border border-slate-700">
                    <i class="ph-bold ph-clock-clockwise text-lg"></i> Sync Time Now
                </button>
            </div>
            <div class="flex gap-3 mt-8">
                <button onclick="closeClockSettings()" class="flex-1 bg-slate-800 hover:bg-slate-700 text-slate-300 py-3 rounded-xl font-bold transition-colors">Cancel</button>
                <button onclick="saveClockSettings()" class="flex-1 bg-gradient-to-r from-cyan-600 to-blue-600 hover:from-cyan-500 hover:to-blue-500 text-white py-3 rounded-xl font-bold shadow-lg transition-all">Save</button>
            </div>
        </div>
    </div>

    <div id="studio-modal" class="fixed inset-0 z-[60] bg-slate-950 hidden flex-col">
        <!-- Step 1: Crop -->
        <div id="step-crop" class="flex flex-col h-full">
            <div class="w-full p-3 flex justify-between items-center bg-slate-900/90 backdrop-blur-sm border-b border-slate-800">
                <div class="flex items-center gap-3">
                    <h3 class="text-cyan-400 font-bold flex items-center gap-2 text-sm"><i class="ph-fill ph-crop"></i> STEP 1: CROP</h3>
                    <div class="flex gap-1 text-xs">
                        <span class="w-6 h-6 rounded-full bg-cyan-500 text-white flex items-center justify-center font-bold">1</span>
                        <span class="w-6 h-6 rounded-full bg-slate-700 text-slate-400 flex items-center justify-center">2</span>
                        <span class="w-6 h-6 rounded-full bg-slate-700 text-slate-400 flex items-center justify-center">3</span>
                    </div>
                </div>
                <div class="flex bg-slate-800 rounded-lg p-1 border border-slate-700 gap-1">
                    <button onclick="rotate(-90)" class="w-8 h-8 rounded-md flex items-center justify-center text-slate-300 hover:text-white hover:bg-slate-700"><i class="ph-bold ph-arrow-counter-clockwise"></i></button>
                    <button onclick="rotate(90)" class="w-8 h-8 rounded-md flex items-center justify-center text-slate-300 hover:text-white hover:bg-slate-700"><i class="ph-bold ph-arrow-clockwise"></i></button>
                </div>
            </div>
            <div class="flex-1 w-full flex items-center justify-center bg-black/90 overflow-hidden p-2">
                <img id="preview-img" class="max-w-full max-h-full block opacity-0"> 
            </div>
            <div class="w-full p-4 flex gap-3 bg-slate-950 border-t border-slate-800">
                <button onclick="closeStudio()" class="flex-1 bg-slate-800 text-slate-300 py-3 rounded-xl font-bold text-sm">Cancel</button>
                <button onclick="goToStep(2)" class="flex-[2] bg-gradient-to-r from-cyan-600 to-blue-600 text-white py-3 rounded-xl font-bold text-sm shadow-[0_0_15px_rgba(6,182,212,0.3)]">Next: Filters <i class="ph-bold ph-arrow-right"></i></button>
            </div>
        </div>

        <!-- Step 2: Filter Presets -->
        <div id="step-filters" class="flex flex-col h-full hidden">
            <div class="w-full p-3 flex justify-between items-center bg-slate-900/90 backdrop-blur-sm border-b border-slate-800">
                <div class="flex items-center gap-3">
                    <h3 class="text-cyan-400 font-bold flex items-center gap-2 text-sm"><i class="ph-fill ph-magic-wand"></i> STEP 2: FILTERS</h3>
                    <div class="flex gap-1 text-xs">
                        <span class="w-6 h-6 rounded-full bg-green-500 text-white flex items-center justify-center"><i class="ph-bold ph-check text-xs"></i></span>
                        <span class="w-6 h-6 rounded-full bg-cyan-500 text-white flex items-center justify-center font-bold">2</span>
                        <span class="w-6 h-6 rounded-full bg-slate-700 text-slate-400 flex items-center justify-center">3</span>
                    </div>
                </div>
            </div>
            <div class="flex-1 w-full flex items-center justify-center bg-black/90 overflow-hidden p-2">
                <img id="filter-preview-img" class="max-w-full max-h-full block rounded-lg shadow-xl"> 
            </div>
            <div class="w-full bg-slate-900 border-t border-slate-800 p-4">
                <div class="grid grid-cols-4 gap-2">
                    <button onclick="applyFilter('none')" class="filter-btn active bg-slate-700 text-white py-2.5 px-2 rounded-lg text-xs font-medium" data-filter="none">Original</button>
                    <button onclick="applyFilter('sepia')" class="filter-btn bg-slate-800 hover:bg-slate-700 text-slate-300 py-2.5 px-2 rounded-lg text-xs font-medium" data-filter="sepia">Sepia</button>
                    <button onclick="applyFilter('bw')" class="filter-btn bg-slate-800 hover:bg-slate-700 text-slate-300 py-2.5 px-2 rounded-lg text-xs font-medium" data-filter="bw">B&W</button>
                    <button onclick="applyFilter('vintage')" class="filter-btn bg-slate-800 hover:bg-slate-700 text-slate-300 py-2.5 px-2 rounded-lg text-xs font-medium" data-filter="vintage">Vintage</button>
                    <button onclick="applyFilter('warm')" class="filter-btn bg-slate-800 hover:bg-slate-700 text-slate-300 py-2.5 px-2 rounded-lg text-xs font-medium" data-filter="warm">Warm</button>
                    <button onclick="applyFilter('cool')" class="filter-btn bg-slate-800 hover:bg-slate-700 text-slate-300 py-2.5 px-2 rounded-lg text-xs font-medium" data-filter="cool">Cool</button>
                    <button onclick="applyFilter('dramatic')" class="filter-btn bg-slate-800 hover:bg-slate-700 text-slate-300 py-2.5 px-2 rounded-lg text-xs font-medium" data-filter="dramatic">Drama</button>
                    <button onclick="applyFilter('soft')" class="filter-btn bg-slate-800 hover:bg-slate-700 text-slate-300 py-2.5 px-2 rounded-lg text-xs font-medium" data-filter="soft">Soft</button>
                </div>
            </div>
            <div class="w-full p-4 flex gap-3 bg-slate-950 border-t border-slate-800">
                <button onclick="goToStep(1)" class="flex-1 bg-slate-800 text-slate-300 py-3 rounded-xl font-bold text-sm"><i class="ph-bold ph-arrow-left"></i> Back</button>
                <button onclick="goToStep(3)" class="flex-[2] bg-gradient-to-r from-cyan-600 to-blue-600 text-white py-3 rounded-xl font-bold text-sm shadow-[0_0_15px_rgba(6,182,212,0.3)]">Next: Adjust <i class="ph-bold ph-arrow-right"></i></button>
            </div>
        </div>

        <!-- Step 3: Manual Adjustments & Upload -->
        <div id="step-adjust" class="flex flex-col h-full hidden">
            <div class="w-full p-3 flex justify-between items-center bg-slate-900/90 backdrop-blur-sm border-b border-slate-800">
                <div class="flex items-center gap-3">
                    <h3 class="text-cyan-400 font-bold flex items-center gap-2 text-sm"><i class="ph-fill ph-sliders"></i> STEP 3: ADJUST</h3>
                    <div class="flex gap-1 text-xs">
                        <span class="w-6 h-6 rounded-full bg-green-500 text-white flex items-center justify-center"><i class="ph-bold ph-check text-xs"></i></span>
                        <span class="w-6 h-6 rounded-full bg-green-500 text-white flex items-center justify-center"><i class="ph-bold ph-check text-xs"></i></span>
                        <span class="w-6 h-6 rounded-full bg-cyan-500 text-white flex items-center justify-center font-bold">3</span>
                    </div>
                </div>
                <button onclick="resetFilters()" class="text-xs bg-slate-800 hover:bg-slate-700 text-slate-300 py-1.5 px-3 rounded-lg flex items-center gap-1">
                    <i class="ph-bold ph-arrow-counter-clockwise"></i> Reset
                </button>
            </div>
            <div class="flex-1 w-full flex items-center justify-center bg-black/90 overflow-hidden p-2">
                <img id="adjust-preview-img" class="max-w-full max-h-full block rounded-lg shadow-xl"> 
            </div>
            <div class="w-full bg-slate-900 border-t border-slate-800 p-4 space-y-3">
                <div class="flex items-center gap-3">
                    <span class="text-xs text-slate-400 w-20">Brightness</span>
                    <input type="range" id="brightnessSlider" min="-50" max="50" value="0" class="flex-1 accent-cyan-500" oninput="adjustBrightness(this.value)">
                    <span id="brightnessVal" class="text-xs text-slate-500 w-8 text-right">0</span>
                </div>
                <div class="flex items-center gap-3">
                    <span class="text-xs text-slate-400 w-20">Contrast</span>
                    <input type="range" id="contrastSlider" min="-50" max="50" value="0" class="flex-1 accent-cyan-500" oninput="adjustContrast(this.value)">
                    <span id="contrastVal" class="text-xs text-slate-500 w-8 text-right">0</span>
                </div>
                <div class="flex items-center gap-3">
                    <span class="text-xs text-slate-400 w-20">Saturation</span>
                    <input type="range" id="saturationSlider" min="-50" max="50" value="0" class="flex-1 accent-cyan-500" oninput="adjustSaturation(this.value)">
                    <span id="saturationVal" class="text-xs text-slate-500 w-8 text-right">0</span>
                </div>
            </div>
            <div class="w-full p-4 flex gap-3 bg-slate-950 border-t border-slate-800">
                <button onclick="goToStep(2)" class="flex-1 bg-slate-800 text-slate-300 py-3 rounded-xl font-bold text-sm"><i class="ph-bold ph-arrow-left"></i> Back</button>
                <button onclick="startRender()" class="flex-[2] bg-gradient-to-r from-green-600 to-emerald-600 text-white py-3 rounded-xl font-bold text-sm shadow-[0_0_15px_rgba(16,185,129,0.3)]"><i class="ph-bold ph-upload"></i> Upload Photo</button>
            </div>
        </div>
    </div>

    <div id="gif-studio-modal" class="fixed inset-0 z-[60] bg-slate-950 hidden flex-col items-center justify-center">
        <div class="absolute top-0 w-full p-4 flex justify-between items-center bg-slate-900/90 backdrop-blur-sm z-10 border-b border-slate-800">
            <h3 class="text-cyan-400 font-bold flex items-center gap-2 text-sm tracking-wide"><i class="ph-fill ph-film-strip"></i> EDIT GIF</h3>
            <div class="flex bg-slate-800 rounded-lg p-1 border border-slate-700 gap-1">
                <button onclick="rotateGif(-90)" class="w-8 h-8 rounded-md flex items-center justify-center text-slate-300 hover:text-white hover:bg-slate-700 transition-all"><i class="ph-bold ph-arrow-counter-clockwise"></i></button>
                <button onclick="rotateGif(90)" class="w-8 h-8 rounded-md flex items-center justify-center text-slate-300 hover:text-white hover:bg-slate-700 transition-all"><i class="ph-bold ph-arrow-clockwise"></i></button>
            </div>
        </div>
        <div class="flex-1 w-full flex items-center justify-center bg-black/90 overflow-hidden">
             <div class="h-[70vh] w-full p-4">
                 <img id="gif-preview-img" class="max-w-full max-h-full block opacity-0"> 
             </div>
        </div>
        <div class="w-full max-w-md pb-8 pt-4 px-6 flex gap-4 z-10 bg-slate-950 border-t border-slate-800">
            <button onclick="closeGifStudio()" class="flex-1 bg-slate-800 text-slate-300 py-3.5 rounded-xl font-bold text-sm">Cancel</button>
            <button onclick="startGifBake()" class="flex-[2] bg-gradient-to-r from-cyan-600 to-blue-600 text-white py-3.5 rounded-xl font-bold text-sm shadow-[0_0_20px_rgba(6,182,212,0.3)]">Bake GIF</button>
        </div>
    </div>
    
    <div id="devices-modal" class="fixed inset-0 z-50 bg-slate-950/90 hidden flex items-center justify-center p-6 backdrop-blur-md">
        <div class="glass-panel w-full max-w-sm rounded-2xl p-6 border-slate-700 shadow-2xl">
            <h3 class="text-lg font-bold text-white mb-6 flex items-center gap-2"><i class="ph-fill ph-devices text-cyan-400"></i> My Devices</h3>
            <div class="space-y-3 mb-4 max-h-64 overflow-y-auto" id="devices-list">
                <div class="bg-slate-900/50 rounded-xl p-3 border border-cyan-800/50 flex items-center justify-between">
                    <div class="flex items-center gap-3">
                        <div class="bg-cyan-900/30 p-2 rounded-lg text-cyan-400"><i class="ph-fill ph-check-circle text-lg"></i></div>
                        <div>
                            <div class="text-white font-bold text-sm" id="current-device-ip">Loading...</div>
                            <div class="text-[10px] text-cyan-400 font-medium">Current Device</div>
                        </div>
                    </div>
                </div>
            </div>
            <div class="space-y-3">
                <div>
                    <label class="text-xs text-slate-400 font-bold ml-1 mb-1 block">Add Device IP Address</label>
                    <input type="text" id="newDeviceIp" placeholder="192.168.1.100" class="w-full bg-slate-900 border border-slate-700 rounded-xl p-3 text-white focus:border-cyan-500 outline-none transition-colors">
                </div>
                <button onclick="addDevice()" class="w-full bg-gradient-to-r from-cyan-600 to-blue-600 hover:from-cyan-500 hover:to-blue-500 text-white py-3 rounded-xl font-bold shadow-lg transition-all">Add Device</button>
            </div>
            <button onclick="document.getElementById('devices-modal').classList.add('hidden')" class="w-full mt-4 bg-slate-800 hover:bg-slate-700 text-slate-300 py-3 rounded-xl font-bold transition-colors">Close</button>
        </div>
    </div>
    
    <div id="toast" class="fixed bottom-24 left-1/2 -translate-x-1/2 glass-panel pl-4 pr-6 py-3 rounded-full text-sm font-bold text-emerald-400 shadow-2xl translate-y-60 transition-transform duration-500 flex items-center gap-3 pointer-events-none border-emerald-500/20 bg-slate-900 border z-[110]">
        <i class="ph-fill ph-check-circle text-xl"></i> <span id="toast-msg">Success</span>
    </div>

<script>
    let currentOrient='landscape', currentTab='photos';
    let isSlideshowActive=false, galleryFiles=[];
    let selectedFile=null, cropAspect='landscape';
    let cropper = null;
    let gifCropper = null;
    let parsedGifData = null;
    let gifRotation = 0;
    let deviceBootId = null, isConnected = true;
    let sortableInstance = null;

    window.onload = () => {
        updateClock();
        setInterval(updateClock, 1000);
        fetchStatus(true);
        setInterval(() => fetchStatus(false), 3000); 
        setOrientation('landscape', false);
        switchTab('photos');
        loadDevices(); 
    };

    function setBusy(state, text="TRANSMITTING...") {
        const ol = document.getElementById('busy-overlay');
        document.getElementById('busy-text').innerText = text;
        if(state) ol.classList.remove('hidden'), ol.classList.add('flex');
        else ol.classList.add('hidden'), ol.classList.remove('flex');
    }

    function showToast(msg) {
        const t = document.getElementById('toast');
        document.getElementById('toast-msg').innerText = msg;
        t.classList.remove('translate-y-60');
        setTimeout(() => t.classList.add('translate-y-60'), 2500);
    }

    async function apiCall(url, options={}) {
        if(url.includes('/upload')) setBusy(true, "UPLOADING...");
        else if(url.includes('delete') || url.includes('order')) setBusy(true, "SAVING...");
        else if(url.includes('/set?orientation')) setBusy(true, "SWITCHING MODE...");
        try {
            const res = await fetch(url, options);
            if(!res.ok) throw new Error(res.status);
            const text = await res.text();
            try { return JSON.parse(text); } catch(e) { return {success:true}; }
        } catch(e) {
            console.error(e);
            return {error: true};
        } finally {
            setBusy(false);
        }
    }

    function updateClock() {
        const now = new Date();
        const timeOptions = {
            hour: '2-digit', 
            minute: '2-digit',
            hour12: !currentClockSettings.use24Hour
        };
        document.getElementById('clock-display').innerText = now.toLocaleTimeString([], timeOptions);
        document.getElementById('date-display').innerText = now.toLocaleDateString([], {weekday:'long', month:'short', day:'numeric'});
    }

    function fetchStatus(isInit = false) {
        fetch('/api/status').then(r=>r.json()).then(d => {
            if (!isConnected) { isConnected = true; setBusy(false); if (!isInit) location.reload(); }
            if (deviceBootId === null) deviceBootId = d.bootId;
            else if (deviceBootId !== d.bootId) location.reload();
            document.getElementById('ip-addr').innerText = window.location.hostname;
            isSlideshowActive = d.slideshow;
            if(isInit && d.orientation) setOrientation(d.orientation, false);
            updateSlideshowBtn();
        }).catch(()=> { 
            isConnected = false; setBusy(true, "RECONNECTING...");
        });
    }

    function switchTab(t) {
        currentTab = t;
        const tp = document.getElementById('tab-photos');
        const tg = document.getElementById('tab-gifs');
        const cPhotos = document.getElementById('controls-photos');
        const cGifs = document.getElementById('controls-gifs');
        const fInput = document.getElementById('fileInput');

        if (t === 'photos') {
            tp.className = "tab-active text-sm font-bold py-3 transition-all flex items-center justify-center gap-2 rounded-t-xl";
            tg.className = "tab-inactive text-sm font-bold py-3 transition-all flex items-center justify-center gap-2 rounded-t-xl hover:text-slate-300";
            cPhotos.classList.remove('hidden');
            cGifs.classList.add('hidden');
            fInput.accept = "image/jpeg,image/png,image/jpg";
        } else {
            tp.className = "tab-inactive text-sm font-bold py-3 transition-all flex items-center justify-center gap-2 rounded-t-xl hover:text-slate-300";
            tg.className = "tab-active text-sm font-bold py-3 transition-all flex items-center justify-center gap-2 rounded-t-xl";
            cPhotos.classList.add('hidden');
            cGifs.classList.remove('hidden');
            fInput.accept = "image/gif";
        }
        loadGallery(true);
    }

    function toggleOrientation() {
        // Toggle between landscape and portrait
        const newOrientation = currentOrient === 'landscape' ? 'portrait' : 'landscape';
        setOrientation(newOrientation);
    }

    function setOrientation(o, refresh=true) {
        currentOrient = o;
        // Update old position buttons (if they exist for backward compatibility)
        const btnL = document.getElementById('btn-land'), btnP = document.getElementById('btn-port');
        // Update new single orientation button
        const btnOrientation = document.getElementById('btn-orientation');
        const orientationText = document.getElementById('orientation-text');
        const label = document.getElementById('current-view-label');
        
        if (o === 'landscape') {
            // Currently landscape, button should show "Portrait" (switch to portrait)
            orientationText.innerText = 'Portrait';
            // Old buttons (backward compatibility)
            if(btnL) btnL.className="px-4 py-1.5 rounded-md text-xs font-bold transition-all bg-cyan-600 text-white shadow-[0_0_15px_rgba(8,145,178,0.4)]";
            if(btnP) btnP.className="px-4 py-1.5 rounded-md text-xs font-bold transition-all text-slate-400 hover:text-white hover:bg-slate-800";
            label.innerText = "Landscape Gallery";
        } else {
            // Currently portrait, button should show "Landscape" (switch to landscape)
            orientationText.innerText = 'Landscape';
            // Old buttons (backward compatibility)
            if(btnL) btnL.className="px-4 py-1.5 rounded-md text-xs font-bold transition-all text-slate-400 hover:text-white hover:bg-slate-800";
            if(btnP) btnP.className="px-4 py-1.5 rounded-md text-xs font-bold transition-all bg-cyan-600 text-white shadow-[0_0_15px_rgba(8,145,178,0.4)]";
            label.innerText = "Portrait Gallery";
        }
        
        // Send orientation change to device (affects all modes: photos, GIFs, clock)
        apiCall('/api/set?orientation=' + o).then(() => {
            loadGallery(refresh);
        });
    }

    function loadGallery(forceRefresh = false) {
        // Reset Header State on Load
        document.getElementById('btn-refresh').classList.remove('hidden');
        document.getElementById('btn-actions').classList.add('hidden');

        const url = '/api/list?_=' + (forceRefresh ? Date.now() : '0');
        fetch(url).then(r=>r.json()).then(data => {
            const grid=document.getElementById('gallery-grid'), empty=document.getElementById('empty-state');
            grid.innerHTML='';
            
            // Filter by:
            // 1. Current Orientation (Landscape vs Portrait)
            // 2. Current Tab (Photos vs GIFs)
            galleryFiles = data.files.filter(f => {
                const isInFolder = f.includes('/' + currentOrient + '/');
                if (!isInFolder) return false;

                if (currentTab === 'photos') {
                    // Show only Images from /photos/ folder
                    const isPhoto = f.toLowerCase().endsWith('.jpg') || f.toLowerCase().endsWith('.jpeg');
                    const isInPhotosFolder = f.includes('/photos/');
                    return isPhoto && isInPhotosFolder;
                } else {
                    // Show only Baked Videos from /gifs/ folder
                    return f.toLowerCase().endsWith('.vid') && f.includes('/gifs/');
                }
            });
            
            if (galleryFiles.length === 0) empty.classList.remove('hidden');
            else {
                empty.classList.add('hidden');
                galleryFiles.forEach((f, idx) => {
                    const isVid = f.toLowerCase().endsWith('.vid');
                    const bust = forceRefresh ? '&t='+Date.now() : '';
                    const name = f.split('/').pop();
                    
                    let thumbUrl;
                    if (isVid) {
                        const thumbPath = f.replace('.vid', '.jpg');
                        thumbUrl = '/api/photo/preview?filename=' + encodeURIComponent(thumbPath) + bust;
                    } else {
                        thumbUrl = '/api/photo/preview?filename=' + encodeURIComponent(f) + bust;
                    }

                    const thumbContent = `<img src="${thumbUrl}" class="w-full h-full object-cover bg-slate-900 pointer-events-none" loading="lazy">`;
                    const el=document.createElement('div');
                    el.className="bg-slate-900 rounded-xl overflow-hidden border border-slate-700 shadow-lg select-none no-context-menu relative group transition-transform hover:scale-[1.02]";
                    el.dataset.file=f;
                    
                    const videoOverlay = isVid ? '<div class="absolute inset-0 flex items-center justify-center pointer-events-none"><div class="bg-black/50 rounded-full p-2"><i class="ph-fill ph-play text-white text-2xl"></i></div></div>' : '';
                    
                    const actionButtons = isVid ? `
                        <div class="flex border-t border-slate-800 divide-x divide-slate-800 bg-slate-900 h-10">
                            <button onclick="playFile('${f}')" class="flex-1 text-cyan-400 hover:bg-slate-800 active:bg-slate-700 transition-colors flex items-center justify-center"><i class="ph-fill ph-play text-lg"></i></button>
                            <button onclick="deleteFile('${f}')" class="flex-1 text-red-400 hover:bg-slate-800 active:bg-slate-700 transition-colors flex items-center justify-center"><i class="ph-bold ph-trash text-lg"></i></button>
                        </div>
                    ` : `
                        <div class="flex border-t border-slate-800 divide-x divide-slate-800 bg-slate-900 h-10">
                            <button onclick="playFile('${f}')" class="flex-1 text-cyan-400 hover:bg-slate-800 active:bg-slate-700 transition-colors flex items-center justify-center"><i class="ph-fill ph-play text-lg"></i></button>
                            <a href="/api/photo/preview?filename=${encodeURIComponent(f)}" download="${name}" class="flex-1 text-emerald-400 hover:bg-slate-800 active:bg-slate-700 transition-colors flex items-center justify-center"><i class="ph-bold ph-download-simple text-lg"></i></a>
                            <button onclick="deleteFile('${f}')" class="flex-1 text-red-400 hover:bg-slate-800 active:bg-slate-700 transition-colors flex items-center justify-center"><i class="ph-bold ph-trash text-lg"></i></button>
                        </div>
                    `;
                    el.innerHTML=`
                        <div class="drag-handle group cursor-grab active:cursor-grabbing relative h-32 w-full">
                            ${thumbContent}
                            ${videoOverlay}
                            <div class="absolute inset-0 bg-black/0 group-hover:bg-black/20 transition-colors"></div>
                        </div>
                        ${actionButtons}
                    `;
                    grid.appendChild(el);
                });
                if(sortableInstance) sortableInstance.destroy();
                // NO Autosave onEnd. Trigger "Dirty State" UI in header instead.
                sortableInstance = new Sortable(grid, { 
                    handle: '.drag-handle', 
                    animation: 200, 
                    delay: 200, 
                    delayOnTouchOnly: true, 
                    ghostClass: 'sortable-ghost', 
                    dragClass: 'sortable-drag', 
                    chosenClass: 'sortable-chosen', 
                    onStart: () => { if(navigator.vibrate) navigator.vibrate(50); }, 
                    onEnd: () => showSaveOptions() 
                });
            }
        });
    }

    function showSaveOptions() { 
        document.getElementById('btn-refresh').classList.add('hidden');
        document.getElementById('btn-actions').classList.remove('hidden');
    }

    function cancelReorder() { 
        loadGallery(false); 
        showToast("Reorder Cancelled"); 
    }

    let photoOrderJustSaved = false; // Flag to track if order was just saved
    
    function saveOrder(){ 
        const grid=document.getElementById('gallery-grid'); 
        const newOrder=Array.from(grid.children).map(el=>el.dataset.file); 
        photoOrderJustSaved = true; // Set flag before saving
        apiCall('/api/order',{method:'POST',body:JSON.stringify({files:newOrder,orientation:currentOrient})})
        .then(() => {
            showToast('Order Saved');
            // Force refresh with cache busting to get the new order from server
            loadGallery(true);
        }); 
    }
    
    function playFile(f){
        const fileOrient = f.includes('/portrait/') ? 'portrait' : 'landscape';
        const isPhoto = f.includes('/photos/');
        
        // Enable slideshow button when playing a photo, disable for GIFs
        if(isPhoto && currentTab === 'photos') {
            enableSlideshowBtn();
        } else {
            disableSlideshowBtn();
        }
        
        apiCall('/api/set?orientation=' + fileOrient).then(() => {
            apiCall('/api/play?file=' + encodeURIComponent(f)).then(() => showToast('Playing Media'));
        });
    }
    
    let slideshowBtnCooldown = false;
    
    function toggleSlideshow(){
        if(slideshowBtnCooldown) return; // Prevent button spam
        
        const btn = document.getElementById('btn-slideshow');
        if(btn.disabled) return;
        
        const action=isSlideshowActive?'stop':'start';
        
        // Set cooldown
        slideshowBtnCooldown = true;
        btn.disabled = true;
        btn.classList.add('opacity-50', 'cursor-not-allowed');
        
        // When starting slideshow in Photos tab, set device orientation based on current folder view
        if(!isSlideshowActive && currentTab === 'photos') {
            apiCall('/api/set?orientation=' + currentOrient).then(() => {
                return apiCall('/api/slideshow/'+action);
            }).then(()=>{
                isSlideshowActive=!isSlideshowActive;
                updateSlideshowBtn();
                showToast('Slideshow '+(isSlideshowActive?'Started':'Stopped'));
                
                // Remove cooldown after 2 seconds
                setTimeout(() => {
                    slideshowBtnCooldown = false;
                    btn.disabled = false;
                    btn.classList.remove('opacity-50', 'cursor-not-allowed');
                }, 2000);
            });
        } else {
            apiCall('/api/slideshow/'+action).then(()=>{
                isSlideshowActive=!isSlideshowActive;
                updateSlideshowBtn();
                showToast('Slideshow '+(isSlideshowActive?'Started':'Stopped'));
                
                // Remove cooldown after 2 seconds
                setTimeout(() => {
                    slideshowBtnCooldown = false;
                    btn.disabled = false;
                    btn.classList.remove('opacity-50', 'cursor-not-allowed');
                }, 2000);
            });
        }
    }
    
    function enableSlideshowBtn() {
        const btn = document.getElementById('btn-slideshow');
        const hint = document.getElementById('slideshow-hint');
        btn.disabled = false;
        btn.className = "bg-slate-800 hover:bg-slate-700 border border-slate-700 text-white py-3.5 rounded-xl flex flex-col items-center gap-1 transition-all active:scale-95 group";
        const icon = document.getElementById('icon-slideshow');
        icon.className = "ph-fill ph-play-circle text-2xl text-emerald-400 transition-colors";
        if(hint) hint.classList.add('hidden');
    }
    
    function disableSlideshowBtn() {
        const btn = document.getElementById('btn-slideshow');
        const hint = document.getElementById('slideshow-hint');
        btn.disabled = true;
        btn.className = "bg-slate-800/50 border border-slate-700 text-slate-500 py-3.5 rounded-xl flex flex-col items-center gap-1 transition-all cursor-not-allowed group";
        const icon = document.getElementById('icon-slideshow');
        icon.className = "ph-fill ph-play-circle text-2xl text-slate-500 transition-colors";
        if(hint) hint.classList.remove('hidden');
    }
    
    function showClock() {
        // Force immediate clock update on device
        apiCall('/api/clock/show').then(() => {
            disableSlideshowBtn(); // Disable slideshow when clock is showing
            showToast('Clock Displayed');
            // Trigger one more update after a brief delay to ensure everything is drawn
            setTimeout(() => {
                apiCall('/api/clock/show');
            }, 100);
        });
    }
    
    function syncTime() {
        apiCall('/api/sync').then(() => {
            showToast('Time Synced');
        });
    }
    
    function updateSlideshowBtn(){
        const txt=document.getElementById('text-slideshow'), icon=document.getElementById('icon-slideshow');
        if(isSlideshowActive){
            txt.innerText="Stop on Current";
            icon.className="ph-fill ph-stop-circle text-2xl text-red-400 animate-pulse";
        }else{
            txt.innerText="Start Slideshow";
            icon.className="ph-fill ph-play-circle text-2xl text-emerald-400";
        }
    }
    
    function deleteFile(f){ 
        if(confirm('Delete this file?')) { 
            apiCall('/api/photo/delete',{method:'POST',body:JSON.stringify({filename:f})}).then(()=>{ 
                if(f.toLowerCase().endsWith('.vid')) {
                    const thumbPath = f.replace('.vid', '.jpg');
                    apiCall('/api/photo/delete',{method:'POST',body:JSON.stringify({filename:thumbPath})});
                }
                showToast('Deleted'); 
                loadGallery(true); 
            }); 
        } 
    }
    function saveDur(){const d=document.getElementById('dur-input').value;apiCall('/api/set?slideshow=true&duration='+d).then(()=>showToast('Duration Saved'));}
    function updateWiFi(){const s=document.getElementById('wifiSsid').value,p=document.getElementById('wifiPass').value;if(!s||!p)return;if(confirm('Save & Reboot?')){document.getElementById('wifi-modal').classList.add('hidden');apiCall('/api/wifi',{method:'POST',body:JSON.stringify({ssid:s,pass:p})}).then(()=>showToast("WiFi Updated"));}}

    let currentClockSettings = {use24Hour: false, timezoneOffset: 28800, timezoneName: "UTC+8 Manila", clockDisplayDuration: 30, clockPhotoInterval: 3, clockOnlyScreen: false, clockEnabled: true, clockShowAtEnd: true, barPosition: "top", customMessages: "Creating memories...\nLife is beautiful!\nEnjoy the moments!\nYou're amazing!\nNever give up!"};
    
    function setBarPosition(position) {
        currentClockSettings.barPosition = position;
        const btnTop = document.getElementById('btn-bar-top');
        const btnBottom = document.getElementById('btn-bar-bottom');
        
        if(position === 'top') {
            btnTop.classList.add('bg-slate-700', 'text-white');
            btnTop.classList.remove('text-slate-400');
            btnBottom.classList.remove('bg-slate-700', 'text-white');
            btnBottom.classList.add('text-slate-400');
        } else {
            btnBottom.classList.add('bg-slate-700', 'text-white');
            btnBottom.classList.remove('text-slate-400');
            btnTop.classList.remove('bg-slate-700', 'text-white');
            btnTop.classList.add('text-slate-400');
        }
    }
    
    function updatePhotoIntervalVisibility() {
        const showAtEnd = document.getElementById('clockShowAtEnd').checked;
        const section = document.getElementById('photoIntervalSection');
        if(showAtEnd) {
            section.style.opacity = '0.5';
            section.style.pointerEvents = 'none';
        } else {
            section.style.opacity = '1';
            section.style.pointerEvents = 'auto';
        }
    }
    
    function openClockSettings() {
        // Fetch current settings from device
        apiCall('/api/clock/settings').then(data => {
            if(data) {
                currentClockSettings.use24Hour = data.use24Hour || false;
                currentClockSettings.timezoneOffset = data.timezoneOffset || 28800;
                currentClockSettings.timezoneName = data.timezoneName || "UTC+8 Manila";
                currentClockSettings.clockDisplayDuration = data.clockDisplayDuration || 30;
                currentClockSettings.clockPhotoInterval = data.clockPhotoInterval || 3;
                currentClockSettings.clockOnlyScreen = data.clockOnlyScreen || false;
                currentClockSettings.clockEnabled = data.clockEnabled !== false; // Default true
                currentClockSettings.clockShowAtEnd = data.clockShowAtEnd !== false; // Default true
                currentClockSettings.barPosition = data.barPosition || 'top';
                currentClockSettings.customMessages = data.customMessages || 'Creating memories...\\nLife is beautiful!\\nEnjoy the moments!\\nYou\'re amazing!\\nNever give up!';
                
                // Update UI
                setClockFormat(currentClockSettings.use24Hour);
                document.getElementById('clockTimezone').value = currentClockSettings.timezoneOffset.toString();
                document.getElementById('clockDisplayDuration').value = currentClockSettings.clockDisplayDuration;
                document.getElementById('clockPhotoInterval').value = currentClockSettings.clockPhotoInterval;
                document.getElementById('clockOnlyScreen').checked = currentClockSettings.clockOnlyScreen;
                document.getElementById('clockEnabled').checked = currentClockSettings.clockEnabled;
                document.getElementById('clockShowAtEnd').checked = currentClockSettings.clockShowAtEnd;
                
                // Load bar settings
                setBarPosition(currentClockSettings.barPosition);
                document.getElementById('customMessages').value = currentClockSettings.customMessages.replace(/\\\\n/g, '\n');
                
                // Set up toggle listeners
                document.getElementById('clockShowAtEnd').onchange = updatePhotoIntervalVisibility;
                updatePhotoIntervalVisibility();
            }
            document.getElementById('clock-settings-modal').classList.remove('hidden');
        });
    }
    
    function closeClockSettings() {
        document.getElementById('clock-settings-modal').classList.add('hidden');
    }
    
    function setClockFormat(is24Hr) {
        currentClockSettings.use24Hour = is24Hr;
        const btn12 = document.getElementById('btn-12hr');
        const btn24 = document.getElementById('btn-24hr');
        
        if(is24Hr) {
            btn12.classList.remove('bg-slate-700', 'text-white');
            btn12.classList.add('text-slate-400');
            btn24.classList.add('bg-slate-700', 'text-white');
            btn24.classList.remove('text-slate-400');
        } else {
            btn24.classList.remove('bg-slate-700', 'text-white');
            btn24.classList.add('text-slate-400');
            btn12.classList.add('bg-slate-700', 'text-white');
            btn12.classList.remove('text-slate-400');
        }
    }
    
    function validateMessageLength(textarea) {
        const lines = textarea.value.split('\n');
        const validation = document.getElementById('messageValidation');
        let hasLongLines = false;
        
        for (let line of lines) {
            if (line.length > 20) {
                hasLongLines = true;
                break;
            }
        }
        
        if (hasLongLines) {
            validation.classList.remove('hidden');
        } else {
            validation.classList.add('hidden');
        }
    }
    
    function saveClockSettings() {
        const tzSelect = document.getElementById('clockTimezone');
        const selectedTz = tzSelect.value;
        const selectedTzName = tzSelect.options[tzSelect.selectedIndex].text;
        const displayDuration = parseInt(document.getElementById('clockDisplayDuration').value);
        const photoInterval = parseInt(document.getElementById('clockPhotoInterval').value);
        const clockOnly = document.getElementById('clockOnlyScreen').checked;
        const clockEnabled = document.getElementById('clockEnabled').checked;
        const clockShowAtEnd = document.getElementById('clockShowAtEnd').checked;
        
        const customMessages = document.getElementById('customMessages').value.replace(/\n/g, '\\n');
        
        const settings = {
            use24Hour: currentClockSettings.use24Hour,
            timezoneOffset: parseInt(selectedTz),
            timezoneName: selectedTzName,
            clockDisplayDuration: displayDuration,
            clockPhotoInterval: clockShowAtEnd ? 0 : photoInterval, // 0 means show at end of slideshow
            clockOnlyScreen: clockOnly,
            clockEnabled: clockEnabled,
            clockShowAtEnd: clockShowAtEnd,
            barPosition: currentClockSettings.barPosition,
            customMessages: customMessages
        };
        
        apiCall('/api/clock/settings', {
            method: 'POST',
            body: JSON.stringify(settings)
        }).then(() => {
            // Update the web UI clock immediately with new format
            updateClock();
            closeClockSettings();
            showToast("Clock Settings Saved");
        });
    }

    function handleFileSelect(){
        const file=document.getElementById('fileInput').files[0];
        if(!file)return;
        selectedFile = file;
        document.getElementById('fileInput').value='';
        if(file.type === 'image/gif') { processGif(file); } else { openStudio(); }
    }

    // --- REAL GIF BAKING LOGIC ---
    function processGif(file) {
        setBusy(true, "READING GIF...");
        const reader = new FileReader();
        reader.onload = function(e) {
            const arr = new Uint8Array(e.target.result);
            const isGif = (arr[0]===0x47 && arr[1]===0x49 && arr[2]===0x46);
            if (!isGif) {
                let msg = "Invalid GIF Format.";
                if (arr[0]===0x52 && arr[1]===0x49 && arr[2]===0x46 && arr[3]===0x46 && arr[8]===0x57 && arr[9]===0x45) {
                    msg = "This is a WebP file, not a GIF.";
                }
                alert(msg);
                setBusy(false);
                return;
            }

            let gif;
            try {
                if (typeof parseGIF === 'undefined') throw new Error("GIF decoder not loaded");
                gif = parseGIF(arr.buffer);
                if (!gif.frames || gif.frames.length === 0) throw new Error("No frames found");
            } catch(e) { 
                console.error("GIF Parse Error:", e);
                alert("GIF Error: " + e.message); 
                setBusy(false); 
                return; 
            }

            const width = gif.width;
            const height = gif.height;
            
            setBusy(true, "PREPARING PREVIEW...");

            const canvas = document.createElement('canvas');
            canvas.width = width;
            canvas.height = height;
            const ctx = canvas.getContext('2d');
            ctx.fillStyle = '#000000';
            ctx.fillRect(0, 0, width, height);
            
            const firstFrame = gif.frames[0];
            if (firstFrame.dims.width > 0 && firstFrame.dims.height > 0 && firstFrame.patch) {
                const tempCanvas = document.createElement('canvas');
                tempCanvas.width = firstFrame.dims.width;
                tempCanvas.height = firstFrame.dims.height;
                const tempCtx = tempCanvas.getContext('2d');
                const patchData = new ImageData(firstFrame.patch, firstFrame.dims.width, firstFrame.dims.height);
                tempCtx.putImageData(patchData, 0, 0);
                ctx.drawImage(tempCanvas, firstFrame.dims.left, firstFrame.dims.top);
            }
            
            canvas.toBlob(blob => {
                const firstFrameUrl = URL.createObjectURL(blob);
                setBusy(false);
                openGifStudio(gif, firstFrameUrl);
            }, 'image/png');
        };
        reader.readAsArrayBuffer(file);
    }

    function uploadRawVideo(buffer, origName, thumbnailBlob) {
        setBusy(true, "UPLOADING PREVIEW...");
        
        let base = origName.split('.').slice(0, -1).join('.');
        base = base.replace(/[^a-zA-Z0-9]/g, '_').substring(0, 10); 
        const timestamp = Math.floor(Date.now()/1000);
        const safeName = base + '_' + timestamp + '.vid'; 
        const thumbName = base + '_' + timestamp + '.jpg'; 

        // UPLOAD PREVIEW FIRST to ensure it's available
        if (thumbnailBlob) {
            const thumbFd = new FormData();
            thumbFd.append('file', thumbnailBlob, thumbName);
            const thumbUrl = `/upload?targetOrient=${currentOrient}&gifType=true`;
            
            apiCall(thumbUrl, { method:'POST', body:thumbFd })
            .then(res => {
                if(res.error) {
                    alert("Preview upload failed!");
                    setBusy(false);
                    return;
                }
                
                // Now upload the video
                setBusy(true, "UPLOADING VIDEO...");
                const blob = new Blob([buffer], {type: "application/octet-stream"});
                const url = `/upload?targetOrient=${currentOrient}`;
                const fd = new FormData();
                fd.append('file', blob, safeName);
                
                return apiCall(url, { method:'POST', body:fd });
            })
            .then(res => {
                if(res && res.error) {
                    // If video upload failed, delete the preview
                    alert("Video upload failed! Cleaning up preview...");
                    const thumbPath = `/gifs/${currentOrient}/${thumbName}`;
                    apiCall('/api/photo/delete',{method:'POST',body:JSON.stringify({filename:thumbPath})});
                    return;
                }
                showToast('Video Baked & Uploaded'); 
                loadGallery(true);
            })
            .catch(err => {
                console.error("Upload error:", err);
                alert("Upload failed!");
                setBusy(false);
            });
        } else {
            alert("No preview available - cannot upload GIF");
            setBusy(false);
        }
    }

    let currentStep = 1;
    
    function openStudio(){
        const studio=document.getElementById('studio-modal');
        studio.classList.remove('hidden'); studio.classList.add('flex');
        
        // Reset to step 1
        currentStep = 1;
        showStep(1);
        resetFilters();
        croppedCanvas = null;
        
        const img=document.getElementById('preview-img');
        const objectUrl = URL.createObjectURL(selectedFile);
        img.src = objectUrl;
        img.style.opacity = 0; 
        if(cropper) cropper.destroy();
        img.onload = () => {
            cropAspect = currentOrient; 
            const ratio = currentOrient === 'portrait' ? 320/480 : 480/320;
            cropper = new Cropper(img, { 
                aspectRatio: ratio, 
                viewMode: 1, 
                dragMode: 'move', 
                autoCropArea: 1, 
                cropBoxResizable: false,
                cropBoxMovable: true,
                background: false, 
                guides: true, 
                center: true, 
                highlight: false, 
                toggleDragModeOnDblclick: false, 
                ready() { img.style.opacity = 1; } 
            });
        };
    }
    
    function showStep(step) {
        document.getElementById('step-crop').classList.add('hidden');
        document.getElementById('step-filters').classList.add('hidden');
        document.getElementById('step-adjust').classList.add('hidden');
        document.getElementById('step-' + (step === 1 ? 'crop' : step === 2 ? 'filters' : 'adjust')).classList.remove('hidden');
    }
    
    function goToStep(step) {
        if (step === 2 && currentStep === 1) {
            // Moving from crop to filters - capture cropped canvas
            if (!cropper) return;
            const w = cropAspect === 'landscape' ? 480 : 320;
            const h = cropAspect === 'landscape' ? 320 : 480;
            croppedCanvas = cropper.getCroppedCanvas({ 
                width: w, 
                height: h, 
                imageSmoothingEnabled: true, 
                imageSmoothingQuality: 'high' 
            });
            cropper.destroy();
            cropper = null;
            
            // Update filter preview
            const filterImg = document.getElementById('filter-preview-img');
            filterImg.src = croppedCanvas.toDataURL('image/jpeg', 0.9);
        }
        
        if (step === 3 && currentStep === 2) {
            // Moving from filters to adjust - update adjust preview
            const adjustImg = document.getElementById('adjust-preview-img');
            const filteredCanvas = applyFiltersToCanvas(croppedCanvas);
            adjustImg.src = filteredCanvas.toDataURL('image/jpeg', 0.9);
        }
        
        if (step === 2 && currentStep === 3) {
            // Going back from adjust to filters
            const filterImg = document.getElementById('filter-preview-img');
            const filteredCanvas = applyFiltersToCanvas(croppedCanvas);
            filterImg.src = filteredCanvas.toDataURL('image/jpeg', 0.9);
        }
        
        if (step === 1 && currentStep === 2) {
            // Going back from filters to crop - reinitialize cropper
            const img = document.getElementById('preview-img');
            img.src = URL.createObjectURL(selectedFile);
            img.style.opacity = 0;
            img.onload = () => {
                cropAspect = currentOrient; 
                const ratio = currentOrient === 'portrait' ? 320/480 : 480/320;
                cropper = new Cropper(img, { 
                    aspectRatio: ratio, 
                    viewMode: 1, 
                    dragMode: 'move', 
                    autoCropArea: 1, 
                    cropBoxResizable: false,
                    cropBoxMovable: true,
                    background: false, 
                    guides: true, 
                    center: true, 
                    highlight: false, 
                    toggleDragModeOnDblclick: false, 
                    ready() { img.style.opacity = 1; } 
                });
            };
            croppedCanvas = null;
        }
        
        currentStep = step;
        showStep(step);
    }

    function rotate(deg){ if(cropper) cropper.rotate(deg); }
    function closeStudio(){ 
        document.getElementById('studio-modal').classList.add('hidden'); 
        document.getElementById('studio-modal').classList.remove('flex'); 
        if(cropper) { cropper.destroy(); cropper=null; }
        resetFilters();
        currentStep = 1;
        croppedCanvas = null;
    }
    
    // Filter system variables
    let currentFilters = {
        filter: 'none',
        brightness: 0,
        contrast: 0,
        saturation: 0
    };
    
    function applyFilter(filterType) {
        currentFilters.filter = filterType;
        updateFilterPreview();
        
        // Update button states
        document.querySelectorAll('.filter-btn').forEach(btn => {
            btn.classList.remove('active', 'bg-slate-700', 'text-white');
            btn.classList.add('bg-slate-800', 'text-slate-300');
        });
        const activeBtn = document.querySelector(`[data-filter="${filterType}"]`);
        if (activeBtn) {
            activeBtn.classList.add('active', 'bg-slate-700', 'text-white');
            activeBtn.classList.remove('bg-slate-800', 'text-slate-300');
        }
    }
    
    function adjustBrightness(value) {
        currentFilters.brightness = parseInt(value);
        document.getElementById('brightnessVal').textContent = value;
        updateFilterPreview();
    }
    
    function adjustContrast(value) {
        currentFilters.contrast = parseInt(value);
        document.getElementById('contrastVal').textContent = value;
        updateFilterPreview();
    }
    
    function adjustSaturation(value) {
        currentFilters.saturation = parseInt(value);
        document.getElementById('saturationVal').textContent = value;
        updateFilterPreview();
    }
    
    function updateFilterPreview() {
        if (!croppedCanvas) return;
        const filteredCanvas = applyFiltersToCanvas(croppedCanvas);
        const dataUrl = filteredCanvas.toDataURL('image/jpeg', 0.9);
        
        // Update all preview images
        const filterImg = document.getElementById('filter-preview-img');
        const adjustImg = document.getElementById('adjust-preview-img');
        if (filterImg) filterImg.src = dataUrl;
        if (adjustImg) adjustImg.src = dataUrl;
    }
    
    // Global variables for filter preview
    let croppedCanvas = null;
    let previewImg = null;
    
    function resetFilters() {
        currentFilters = { filter: 'none', brightness: 0, contrast: 0, saturation: 0 };
        const brightnessSlider = document.getElementById('brightnessSlider');
        const contrastSlider = document.getElementById('contrastSlider');
        const saturationSlider = document.getElementById('saturationSlider');
        
        if (brightnessSlider) brightnessSlider.value = 0;
        if (contrastSlider) contrastSlider.value = 0;
        if (saturationSlider) saturationSlider.value = 0;
        
        // Update value labels
        const brightnessVal = document.getElementById('brightnessVal');
        const contrastVal = document.getElementById('contrastVal');
        const saturationVal = document.getElementById('saturationVal');
        if (brightnessVal) brightnessVal.textContent = '0';
        if (contrastVal) contrastVal.textContent = '0';
        if (saturationVal) saturationVal.textContent = '0';
        
        // Reset filter buttons
        document.querySelectorAll('.filter-btn').forEach(btn => {
            btn.classList.remove('active', 'bg-slate-700', 'text-white');
            btn.classList.add('bg-slate-800', 'text-slate-300');
        });
        const originalBtn = document.querySelector('[data-filter="none"]');
        if (originalBtn) {
            originalBtn.classList.add('active', 'bg-slate-700', 'text-white');
            originalBtn.classList.remove('bg-slate-800', 'text-slate-300');
        }
        
        // Update preview
        updateFilterPreview();
    }
    
    // AI features removed - using local filter presets instead
    
    function openGifStudio(gif, firstFrameUrl) {
        parsedGifData = gif;
        gifRotation = 0;
        const studio = document.getElementById('gif-studio-modal');
        studio.classList.remove('hidden'); studio.classList.add('flex');
        const img = document.getElementById('gif-preview-img');
        img.src = firstFrameUrl;
        img.style.opacity = 0;
        if(gifCropper) gifCropper.destroy();
        img.onload = () => {
            const ratio = currentOrient === 'portrait' ? 320/480 : 480/320;
            gifCropper = new Cropper(img, { aspectRatio: ratio, viewMode: 1, dragMode: 'move', autoCropArea: 1, background: false, guides: true, center: true, highlight: false, toggleDragModeOnDblclick: false, ready() { img.style.opacity = 1; } });
        };
    }
    
    function rotateGif(deg) { if(gifCropper) gifCropper.rotate(deg); gifRotation = (gifRotation + deg) % 360; }
    function closeGifStudio() { document.getElementById('gif-studio-modal').classList.add('hidden'); document.getElementById('gif-studio-modal').classList.remove('flex'); if(gifCropper) { gifCropper.destroy(); gifCropper = null; } parsedGifData = null; gifRotation = 0; }
    
    function startGifBake() {
        if(!gifCropper || !parsedGifData) return;
        const cropData = gifCropper.getData();
        const gifData = parsedGifData;
        const rotation = gifRotation;
        document.getElementById('gif-studio-modal').classList.add('hidden');
        document.getElementById('gif-studio-modal').classList.remove('flex');
        if(gifCropper) { gifCropper.destroy(); gifCropper = null; }
        bakeGifWithCrop(gifData, cropData, rotation);
    }

    function bakeGifWithCrop(gif, cropData, rotation) {
        const width = gif.width;
        const height = gif.height;
        const rawFrames = gif.frames;
        const numFrames = rawFrames.length;
        setBusy(true, "BAKING 0/" + numFrames + "...");
        const targetW = currentOrient === 'landscape' ? 480 : 320;
        const targetH = currentOrient === 'landscape' ? 320 : 480;
        const canvas = document.createElement('canvas');
        canvas.width = width;
        canvas.height = height;
        const ctx = canvas.getContext('2d');
        ctx.fillStyle = '#000000';
        ctx.fillRect(0, 0, width, height);
        const tempCanvas = document.createElement('canvas');
        const tempCtx = tempCanvas.getContext('2d');
        const cropCanvas = document.createElement('canvas');
        const cropCtx = cropCanvas.getContext('2d');
        const rCanvas = document.createElement('canvas');
        rCanvas.width = targetW;
        rCanvas.height = targetH;
        const rCtx = rCanvas.getContext('2d');
        const totalSize = numFrames * targetW * targetH * 2;
        const finalBuffer = new Uint8Array(totalSize);
        let offset = 0;
        let thumbnailBlob = null;
        let f = 0;
        function processFrame() {
            if (f >= numFrames) {
                uploadRawVideo(finalBuffer, selectedFile.name, thumbnailBlob);
                return;
            }
            setBusy(true, "BAKING " + (f+1) + "/" + numFrames + "...");
            const frame = rawFrames[f];
            if (f > 0) {
                const prevFrame = rawFrames[f-1];
                if (prevFrame.disposal === 2) {
                    ctx.fillStyle = '#000000';
                    ctx.fillRect(prevFrame.dims.left, prevFrame.dims.top, prevFrame.dims.width, prevFrame.dims.height);
                }
            }
            if (frame.dims.width > 0 && frame.dims.height > 0 && frame.patch) {
                tempCanvas.width = frame.dims.width;
                tempCanvas.height = frame.dims.height;
                const patchData = new ImageData(frame.patch, frame.dims.width, frame.dims.height);
                tempCtx.putImageData(patchData, 0, 0);
                ctx.drawImage(tempCanvas, frame.dims.left, frame.dims.top);
            }
            const cw = cropData.width;
            const ch = cropData.height;
            const rot = rotation * Math.PI / 180;
            const isRotated90or270 = (Math.abs(rotation) === 90 || Math.abs(rotation) === 270);
            cropCanvas.width = isRotated90or270 ? ch : cw;
            cropCanvas.height = isRotated90or270 ? cw : ch;
            cropCtx.fillStyle = '#000000';
            cropCtx.fillRect(0, 0, cropCanvas.width, cropCanvas.height);
            cropCtx.save();
            cropCtx.translate(cropCanvas.width/2, cropCanvas.height/2);
            cropCtx.rotate(rot);
            cropCtx.drawImage(canvas, cropData.x, cropData.y, cw, ch, -cw/2, -ch/2, cw, ch);
            cropCtx.restore();
            rCtx.fillStyle = '#000000';
            rCtx.fillRect(0, 0, targetW, targetH);
            rCtx.drawImage(cropCanvas, 0, 0, targetW, targetH);
            const p = rCtx.getImageData(0, 0, targetW, targetH).data;
            for (let i = 0; i < p.length; i += 4) {
                const r = p[i];
                const g = p[i+1];
                const b = p[i+2];
                const rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                finalBuffer[offset++] = rgb565 & 0xFF;
                finalBuffer[offset++] = (rgb565 >> 8) & 0xFF;
            }
            if (f === 0) {
                rCanvas.toBlob(blob => {
                    thumbnailBlob = blob;
                    f++;
                    setTimeout(processFrame, 5);
                }, 'image/jpeg', 0.8);
                return;
            }
            f++;
            setTimeout(processFrame, 5);
        }
        processFrame();
    }
    
    function startRender(){
        let cvs;
        const w=cropAspect==='landscape'?480:320; 
        const h=cropAspect==='landscape'?320:480;
        
        // Use croppedCanvas if available (after finalizeCrop), otherwise get from cropper
        if (croppedCanvas) {
            cvs = croppedCanvas;
        } else if (cropper) {
            cvs = cropper.getCroppedCanvas({ width: w, height: h, imageSmoothingEnabled: true, imageSmoothingQuality: 'high' });
        } else {
            showToast('No image to upload');
            return;
        }
        
        // Apply filters to the final canvas before upload
        const filteredCanvas = applyFiltersToCanvas(cvs);
        filteredCanvas.toBlob(blob => { uploadFile(blob, selectedFile.name); }, 'image/jpeg', 0.92);
    }
    
    function applyFiltersToCanvas(sourceCanvas) {
        const canvas = document.createElement('canvas');
        canvas.width = sourceCanvas.width;
        canvas.height = sourceCanvas.height;
        const ctx = canvas.getContext('2d');
        
        // Apply CSS filters first
        let filterCSS = '';
        switch(currentFilters.filter) {
            case 'sepia':
                filterCSS += 'sepia(100%) ';
                break;
            case 'bw':
                filterCSS += 'grayscale(100%) ';
                break;
            case 'vintage':
                filterCSS += 'sepia(60%) contrast(1.2) brightness(0.9) ';
                break;
            case 'warm':
                filterCSS += 'sepia(30%) saturate(1.3) hue-rotate(-10deg) ';
                break;
            case 'cool':
                filterCSS += 'saturate(1.1) hue-rotate(10deg) brightness(1.1) ';
                break;
            case 'dramatic':
                filterCSS += 'contrast(1.5) saturate(1.3) brightness(0.9) ';
                break;
            case 'soft':
                filterCSS += 'blur(0.5px) brightness(1.1) contrast(0.9) ';
                break;
        }
        
        // Add manual adjustments
        filterCSS += `brightness(${1 + currentFilters.brightness/100}) `;
        filterCSS += `contrast(${1 + currentFilters.contrast/100}) `;
        filterCSS += `saturate(${1 + currentFilters.saturation/100}) `;
        
        ctx.filter = filterCSS;
        ctx.drawImage(sourceCanvas, 0, 0);
        
        return canvas;
    }

    function uploadFile(blob, name) {
        const ext = name.split('.').pop();
        let base = name.split('.').slice(0, -1).join('.');
        base = base.replace(/[^a-zA-Z0-9]/g, '_').substring(0, 15); 
        const safeName = base + '_' + Math.floor(Date.now()/1000) + '.' + ext;
        const url = `/upload?targetOrient=${cropAspect}`;
        const fd = new FormData();
        fd.append('file', blob, safeName);
        apiCall(url, { method:'POST', body:fd }).then(res => { 
            if(res.error) alert("Upload failed!");
            else { showToast('Photo Uploaded'); closeStudio(); loadGallery(true); }
        });
    }

    function loadDevices() {
        const currentIp = window.location.hostname;
        document.getElementById('current-device-ip').innerText = currentIp;
        const saved = JSON.parse(localStorage.getItem('chronosync_devices') || '[]');
        const list = document.getElementById('devices-list');
        const oldDevices = list.querySelectorAll('.saved-device');
        oldDevices.forEach(d => d.remove());
        saved.forEach((ip, idx) => {
            if(ip !== currentIp) { 
                const div = document.createElement('div');
                div.className = 'saved-device bg-slate-900/50 rounded-xl p-3 border border-slate-700 flex items-center justify-between';
                div.innerHTML = `<div class="flex items-center gap-3"><div class="bg-slate-800 p-2 rounded-lg text-slate-400"><i class="ph-fill ph-desktop text-lg"></i></div><div><div class="text-white font-bold text-sm">${ip}</div><div class="text-[10px] text-slate-400 font-medium">Saved Device</div></div></div><div class="flex gap-2"><a href="http://${ip}" target="_blank" class="bg-cyan-900/30 hover:bg-cyan-900/50 text-cyan-400 px-3 py-1.5 rounded-lg text-xs font-bold transition-colors">Open</a><button onclick="removeDevice(${idx})" class="bg-red-900/30 hover:bg-red-900/50 text-red-400 px-3 py-1.5 rounded-lg text-xs font-bold transition-colors">Remove</button></div>`;
                list.appendChild(div);
            }
        });
    }
    
    function addDevice() {
        const input = document.getElementById('newDeviceIp');
        const ip = input.value.trim();
        if(!ip) { alert('Please enter an IP address'); return; }
        const ipPattern = /^(\d{1,3}\.){3}\d{1,3}$/;
        if(!ipPattern.test(ip)) { alert('Please enter a valid IP address (e.g., 192.168.1.100)'); return; }
        const saved = JSON.parse(localStorage.getItem('chronosync_devices') || '[]');
        if(!saved.includes(ip)) { saved.push(ip); localStorage.setItem('chronosync_devices', JSON.stringify(saved)); showToast('Device Added'); input.value = ''; loadDevices(); } else { alert('This device is already in your list'); }
    }
    
    function removeDevice(idx) { if(confirm('Remove this device from your list?')) { const saved = JSON.parse(localStorage.getItem('chronosync_devices') || '[]'); saved.splice(idx, 1); localStorage.setItem('chronosync_devices', JSON.stringify(saved)); showToast('Device Removed'); loadDevices(); } }

    // Initialize the page when DOM is ready
    document.addEventListener('DOMContentLoaded', function() {
        if (typeof loadGallery === 'function') {
            loadGallery(true);
        }
    });

</script>
</body>
</html>
)rawliteral";