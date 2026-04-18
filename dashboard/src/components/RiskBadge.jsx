import React from 'react';
const CFG = {
  safe:        { label:'SAFE',     cls:'b-safe',     dot:'#00ff9d' },
  medium:      { label:'MEDIUM',   cls:'b-medium',   dot:'#ffbb00' },
  high:        { label:'HIGH',     cls:'b-high',     dot:'#ff7700' },
  critical:    { label:'CRITICAL', cls:'b-critical', dot:'#ff2255' },
  unknown:     { label:'UNKNOWN',  cls:'b-unknown',  dot:'#6090b0' },
  whitelisted: { label:'SAFE',     cls:'b-safe',     dot:'#00ff9d' },
  blocked:     { label:'BLOCKED',  cls:'b-blocked',  dot:'#ff2255' },
};
export default function RiskBadge({ level }) {
  const c = CFG[level?.toLowerCase()] || CFG.unknown;
  return (
    <span className={`badge ${c.cls}`}>
      <span style={{ width:8, height:8, borderRadius:'50%', background:c.dot, display:'inline-block', boxShadow:`0 0 6px ${c.dot}` }} />
      {c.label}
    </span>
  );
}
