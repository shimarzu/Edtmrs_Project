import React, { useState, useEffect, useCallback } from 'react';
import { endpointsAPI } from '../utils/api';

export default function EndpointsPage({ liveEvents }) {
  const [endpoints, setEndpoints] = useState([]);
  const [loading, setLoading] = useState(true);
  const [toast, setToast] = useState('');

  const load = useCallback(async () => { try { const r = await endpointsAPI.list(); setEndpoints(r.data.endpoints); } catch {} setLoading(false); }, []);
  useEffect(() => { load(); }, [load]);
  useEffect(() => { if (liveEvents?.length) load(); }, [liveEvents, load]);

  const isolate = async ep => {
    const r = window.prompt('Isolate ' + ep.hostname + '? Reason:', 'Suspicious activity');
    if (r === null) return;
    try { await endpointsAPI.isolate(ep.id, r); setToast('⚠️ ' + ep.hostname + ' ISOLATED'); setTimeout(() => setToast(''), 3000); load(); } catch {}
  };
  const unisolate = async ep => {
    try { await endpointsAPI.unisolate(ep.id); setToast('✅ ' + ep.hostname + ' RESTORED'); setTimeout(() => setToast(''), 3000); load(); } catch {}
  };

  const online   = endpoints.filter(e => e.status === 'online' && !e.is_isolated).length;
  const offline  = endpoints.filter(e => e.status !== 'online' && !e.is_isolated).length;
  const isolated = endpoints.filter(e => e.is_isolated).length;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
      {toast && <div className="anim-slide-r" style={{ position: 'fixed', top: '20px', right: '20px', zIndex: 300, background: 'rgba(0,200,255,.1)', border: '1.5px solid rgba(0,200,255,.4)', color: 'var(--blue)', fontFamily: 'Orbitron, monospace', fontSize: '11px', letterSpacing: '.1em', padding: '12px 18px' }}>{toast}</div>}

      <div>
        <div className="label-xs" style={{ marginBottom: '6px' }}>MODULE // EPT-001</div>
        <h1 style={{ fontFamily: 'Orbitron, monospace', fontSize: '28px', fontWeight: '800', color: 'var(--text)', letterSpacing: '.05em' }}>🖥️ Endpoint Registry</h1>
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '14px' }}>
        {[
          { emoji: '🟢', label: 'ONLINE',   value: online,   color: 'var(--green)',  bg: 'rgba(0,255,153,.07)',  b: 'rgba(0,255,153,.25)',  dc: 'dot-on'   },
          { emoji: '⚫', label: 'OFFLINE',  value: offline,  color: 'var(--dim)',    bg: 'rgba(58,96,128,.07)', b: 'rgba(58,96,128,.25)', dc: 'dot-off'  },
          { emoji: '🔴', label: 'ISOLATED', value: isolated, color: 'var(--red)',    bg: 'rgba(255,34,85,.07)', b: 'rgba(255,34,85,.25)', dc: 'dot-crit' },
        ].map(s => (
          <div key={s.label} style={{ background: s.bg, border: '1.5px solid ' + s.b, padding: '20px', position: 'relative', overflow: 'hidden' }}>
            <div style={{ position: 'absolute', top: 0, left: 0, right: 0, height: '2px', background: 'linear-gradient(90deg,transparent,' + s.color + ',transparent)' }} />
            <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '12px' }}>
              <span style={{ fontSize: '24px' }}>{s.emoji}</span>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span className={'dot ' + s.dc} />
                <span className="label-sm" style={{ color: s.color, fontSize: '12px' }}>{s.label}</span>
              </div>
            </div>
            <div style={{ fontFamily: 'Orbitron, monospace', fontSize: '44px', fontWeight: '800', color: s.color, textShadow: '0 0 20px ' + s.color + '50' }}>{s.value}</div>
          </div>
        ))}
      </div>

      <div className="panel" style={{ overflow: 'hidden' }}>
        <div style={{ overflowX: 'auto' }}>
          <table className="tbl">
            <thead><tr>{['Status', 'Hostname', 'IP Address', 'User', 'Agent', 'Last Seen', 'Action'].map(h => <th key={h}>{h}</th>)}</tr></thead>
            <tbody>
              {loading ? (
                <tr><td colSpan={7} style={{ textAlign: 'center', padding: '60px', color: 'var(--muted)', fontFamily: 'Share Tech Mono, monospace' }}>🔄 SCANNING NETWORK NODES...</td></tr>
              ) : !endpoints.length ? (
                <tr><td colSpan={7} style={{ textAlign: 'center', padding: '60px', color: 'var(--muted)', fontFamily: 'Share Tech Mono, monospace' }}>📭 NO ENDPOINTS — START AGENT ON USER PC</td></tr>
              ) : endpoints.map(ep => (
                <tr key={ep.id}>
                  <td>
                    {ep.is_isolated
                      ? <span style={{ display: 'inline-flex', alignItems: 'center', gap: '7px', fontFamily: 'Orbitron, monospace', fontSize: '10px', color: 'var(--red)', background: 'rgba(255,34,85,.09)', border: '1px solid rgba(255,34,85,.35)', padding: '5px 10px' }}><span className="dot dot-crit" />ISOLATED</span>
                      : ep.status === 'online'
                        ? <span style={{ display: 'inline-flex', alignItems: 'center', gap: '7px', fontFamily: 'Orbitron, monospace', fontSize: '10px', color: 'var(--green)', background: 'rgba(0,255,153,.07)', border: '1px solid rgba(0,255,153,.3)', padding: '5px 10px' }}><span className="dot dot-on" />ONLINE</span>
                        : <span style={{ display: 'inline-flex', alignItems: 'center', gap: '7px', fontFamily: 'Orbitron, monospace', fontSize: '10px', color: 'var(--dim)', background: 'rgba(58,96,128,.07)', border: '1px solid rgba(58,96,128,.25)', padding: '5px 10px' }}><span className="dot dot-off" />OFFLINE</span>
                    }
                  </td>
                  <td><div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}><span style={{ fontSize: '16px' }}>🖥️</span><span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '13px', fontWeight: '600', color: 'var(--text)' }}>{ep.hostname}</span></div></td>
                  <td style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--cyan)' }}>{ep.ip_address || '—'}</td>
                  <td><div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}><span style={{ fontSize: '14px' }}>👤</span><span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--dim)' }}>{ep.username || '—'}</span></div></td>
                  <td style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--blue)' }}>v{ep.agent_version}</td>
                  <td style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '11px', color: 'var(--muted)', whiteSpace: 'nowrap' }}>{ep.last_seen ? new Date(ep.last_seen).toLocaleString() : 'Never'}</td>
                  <td>
                    {ep.is_isolated
                      ? <button className="btn btn-g" style={{ padding: '6px 14px', fontSize: '10px' }} onClick={() => unisolate(ep)}>♻️ RESTORE</button>
                      : <button className="btn btn-r" style={{ padding: '6px 14px', fontSize: '10px' }} onClick={() => isolate(ep)}>🔒 ISOLATE</button>
                    }
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
}
