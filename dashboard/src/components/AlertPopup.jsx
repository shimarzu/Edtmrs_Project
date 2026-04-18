import React, { useEffect } from 'react';

export default function AlertPopup({ alert, onClose }) {
  useEffect(() => { const t = setTimeout(onClose, 9000); return () => clearTimeout(t); }, [onClose]);

  const isCrit  = alert.risk_level === 'critical';
  const isHigh  = alert.risk_level === 'high';
  const color   = isCrit ? 'var(--red)' : isHigh ? 'var(--orange)' : 'var(--amber)';
  const glowRgb = isCrit ? '255,34,85' : isHigh ? '255,119,0' : '255,187,0';
  const emoji   = isCrit ? '🔴' : isHigh ? '🟠' : '🟡';

  return (
    <div className="anim-slide-r" style={{
      width: '360px',
      background: 'linear-gradient(135deg, #060f1c 0%, #09162a 100%)',
      border: '1.5px solid ' + color + '80',
      boxShadow: '0 0 30px rgba(' + glowRgb + ',.45), 0 8px 32px rgba(0,0,0,.6)',
      position: 'relative', overflow: 'hidden',
    }}>
      <div style={{ height: '3px', background: 'linear-gradient(90deg,transparent,' + color + ',transparent)', animation: 'pulse 1.2s infinite' }} />
      <div style={{ position: 'absolute', top: 0, left: 0, width: '18px', height: '18px', borderTop: '2px solid ' + color, borderLeft: '2px solid ' + color }} />
      <div style={{ position: 'absolute', bottom: 0, right: 0, width: '18px', height: '18px', borderBottom: '2px solid ' + color, borderRight: '2px solid ' + color }} />

      <div style={{ padding: '16px 18px 14px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <span style={{ fontSize: '22px', filter: 'drop-shadow(0 0 8px ' + color + ')' }}>{emoji}</span>
            <div>
              <div style={{ fontFamily: 'Orbitron, monospace', fontSize: '11px', fontWeight: '700', color: color, letterSpacing: '.12em' }}>
                {(alert.risk_level || 'MEDIUM').toUpperCase()} THREAT DETECTED
              </div>
              <div style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '10px', color: 'var(--dim)', marginTop: '2px' }}>
                {new Date().toLocaleTimeString()}
              </div>
            </div>
          </div>
          <button onClick={onClose} style={{ background: 'none', border: 'none', color: 'var(--dim)', cursor: 'pointer', fontSize: '18px', lineHeight: 1, padding: '4px' }}>✕</button>
        </div>

        <div style={{ fontFamily: 'Exo 2, sans-serif', fontSize: '17px', fontWeight: '700', color: 'var(--text)', marginBottom: '12px', letterSpacing: '.02em' }}>
          {alert.device_name || 'Unknown USB Device'}
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
          {[
            { icon: '🖥️', k: 'HOST', v: alert.hostname },
            { icon: '👤', k: 'USER', v: alert.username },
            { icon: '🔌', k: 'VID',  v: alert.vendor_id },
            { icon: '📦', k: 'PID',  v: alert.product_id },
          ].filter(x => x.v && x.v !== 'unknown').map(({ icon, k, v }) => (
            <div key={k} style={{ display: 'flex', alignItems: 'center', gap: '6px', background: 'rgba(0,0,0,.3)', padding: '7px 9px' }}>
              <span style={{ fontSize: '14px' }}>{icon}</span>
              <div>
                <div style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '9px', color: 'var(--dim)', letterSpacing: '.12em' }}>{k}</div>
                <div style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--cyan)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', maxWidth: '100px' }}>{v}</div>
              </div>
            </div>
          ))}
        </div>
      </div>

      <div style={{ height: '3px', background: 'var(--border)', position: 'relative' }}>
        <div style={{ height: '100%', background: color, animation: 'shrink 9s linear forwards' }} />
      </div>
    </div>
  );
}
