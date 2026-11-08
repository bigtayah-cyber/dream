// Génère Source/Orbe/OrbeStyles.h à partir de orbe.html (données pures)
const fs=require('fs');
const src=fs.readFileSync(process.argv[2],'utf8').split('\n');
const a=src.findIndex(l=>l.startsWith('const SCALES')), b=src.findIndex(l=>l.startsWith('/* ---------- state'));
const code=src.slice(a,b).join('\n')+'\nmodule.exports={SCALES,STYLES};';
const m={exports:{}}; new Function('module',code)(m);
const {SCALES,STYLES}=m.exports;
const sk=Object.keys(SCALES);
const F=x=>Number.isInteger(x)?x+'.f':x+'f';
const hits=a=>'{'+(a||[]).map(x=>Array.isArray(x)?`{${F(x[0])},${F(x[1])}}`:`{${F(x)},-1.f}`).join(',')+'}';
const L=[];
L.push('// Fichier généré par tools/gen_styles.js — ne pas modifier à la main.','#pragma once','#include <vector>','namespace orbe {',
'struct Hit { float step; float vel; };  // vel < 0 : vélocité par défaut de la voix',
'struct Drums { std::vector<Hit> K, HK, S, C, R, O, H, kR; int hGrid = 0; float hv = 0.5f; bool roll = false, stut = false; };',
'struct Style { const char* id; const char* group; const char* label; int bpm; int scale; bool seventh;',
' std::vector<Hit> rhythm; const char* cs; const char* ls; const char* bs; std::vector<int> bass;',
' float swing, maxDur, stacc, dBoost; int lOct; bool bShort; Drums dr; std::vector<std::vector<int>> banks; };',
`struct Scale { const char* id; const char* label; int iv[7]; };`,
`inline const std::vector<Scale>& scales() { static const std::vector<Scale> s = {`+
 sk.map(k=>`{"${k}","${SCALES[k].label}",{${SCALES[k].iv.join(',')}}}`).join(',')+`}; return s; }`,
'inline const std::vector<Style>& styles() { static const std::vector<Style> s = {');
const ids=Object.keys(STYLES);
ids.forEach((id,i)=>{const t=STYLES[id],d=t.dr||{};
 const grid=d.H==='8'?8:d.H==='16'?16:0;
 const H=(typeof d.H==='string')?[]:d.H;
 L.push(`{"${id}","${t.g}","${t.label}",${t.bpm},${sk.indexOf(t.scale)},${!!t.seventh},${hits(t.rhythm.map(r=>[r[0],r[1]]))},"${t.cs}","${t.ls}","${t.bs}",{${t.bass.join(',')}},${F(t.swing||0)},${F(t.maxDur)},${F(t.stacc||0)},${F(t.dBoost||0)},${t.lOct||0},${!!t.bShort},`+
 `{${hits(d.K)},${hits(d.HK)},${hits(d.S)},${hits(d.C)},${hits(d.R)},${hits(d.O)},${hits(H)},${hits((d.kR||[]))},${grid},${F(d.hv||0.5)},${!!d.roll},${!!d.stut}},`+
 `{${t.banks.map(x=>'{'+x.join(',')+'}').join(',')}}}${i<ids.length-1?',':''}`);});
L.push('}; return s; }','} // namespace orbe');
fs.writeFileSync('Source/Orbe/OrbeStyles.h',L.join('\n')+'\n');
console.log(ids.length,'styles',sk.length,'gammes; scales:',sk.join(','));
