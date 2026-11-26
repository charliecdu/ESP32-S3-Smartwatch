/*
 * web_interface.h
 * Embedded browser interface for recording playback and downloads
 */
#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>Smartwatch</title>
<style>body{font:17px system-ui,sans-serif;max-width:36rem;margin:2rem auto;padding:0 1rem;background:#f4f6f8;color:#17202a}main{background:white;padding:1.4rem;border-radius:12px}button{font:inherit;margin:.4rem .4rem .4rem 0;padding:.65rem 1rem;border:1px solid #aab5bf;border-radius:7px;background:#edf3fa}button:disabled{opacity:.45}audio{width:100%;margin-top:1rem}small{color:#596573}</style></head>
<body><main><h1>Smartwatch</h1><p>State: <strong id="state">Connecting</strong></p><p id="file">No recording</p><p id="hint"></p><button id="play" disabled>Play WAV</button><button id="save" disabled>Download WAV</button><audio id="audio" controls hidden></audio><p id="message" role="status"></p><small>Unlock on the watch before accessing audio. A new recording replaces the previous note.</small></main>
<script>
const state=document.getElementById('state'), file=document.getElementById('file'), hint=document.getElementById('hint'), play=document.getElementById('play'), save=document.getElementById('save'), audio=document.getElementById('audio'), message=document.getElementById('message');
// Track transfer state and the current playback URL
let busy=false, allowed=false, currentFile='', objectURL=null;
// Release the previous audio URL when the note changes
function clearPlayer(){audio.pause();audio.removeAttribute('src');audio.load();audio.hidden=true;if(objectURL){URL.revokeObjectURL(objectURL);objectURL=null;}}
// Disable actions while access is unavailable or a transfer is running
function buttons(){play.disabled=save.disabled=busy||!allowed;}
// Poll the watch for access changes and the latest recording
async function refresh(){try{const response=await fetch('/status',{cache:'no-store'});if(!response.ok)throw Error('Unavailable');const data=await response.json();state.textContent=data.state;const name=data.filename||'';if(!busy&&(!data.access||currentFile!==name))clearPlayer();currentFile=name;allowed=data.access&&Boolean(name)&&!data.transfer;file.textContent=name?name+' | '+Number(data.duration).toFixed(2)+' s':'No accessible recording';hint.textContent=data.access?'Touch the watch to record.':'Unlock using the selected gesture.';buttons();}catch(e){state.textContent='Disconnected';allowed=false;buttons();}}
// Fetch the WAV for either local playback or download
async function getAudio(download){if(busy||!allowed)return;busy=true;buttons();message.textContent='Receiving audio...';try{const r=await fetch('/audio.wav',{cache:'no-store'});if(!r.ok)throw Error(r.status===403?'Watch is locked':'Audio unavailable');const blob=await r.blob();if(download){const url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=currentFile||'note.wav';document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),10000);}else{clearPlayer();objectURL=URL.createObjectURL(blob);audio.src=objectURL;audio.hidden=false;try{await audio.play();}catch(e){message.textContent='Press play on the audio control.';return;}}message.textContent=download?'Download ready.':'Audio ready.';}catch(e){message.textContent=e.message;}finally{busy=false;await refresh();}}
// Refresh status every two seconds between transfers
play.onclick=()=>getAudio(false);save.onclick=()=>getAudio(true);refresh();setInterval(()=>{if(!busy)refresh();},2000);
</script></body></html>)HTML";
