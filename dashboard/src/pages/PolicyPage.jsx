import React, { useState, useEffect, useCallback } from 'react';
import { actionsAPI } from '../utils/api';

export default function PolicyPage() {
  const [wl, setWl] = useState([]);
  const [bl, setBl] = useState([]);
  const [tab, setTab] = useState('whitelist');
  const [loading, setLoading] = useState(true);
  const [toast, setToast] = useState('');

  const load = useCallback(async () => {
    setLoading(true);
    try { const [w, b] = await Promise.all([actionsAPI.getWhitelist(), actionsAPI.getBlocked()]); setWl(w.data.whitelist); setBl(b.data.blocked_devices); }
    catch {} setLoading(false);
  }, []);

  useEffect(() => { load(); }, [load]);

  const remove  = async id => { await actionsAPI.removeWhitelist(id); setToast('✅ Removed from whitelist'); setTimeout(() => setToast(''), 3000); load(); };
  const unblock = async id => { await actionsAPI.unblockDevice(id);   setToast('✅ Device unblocked');       setTimeout(() => setToast(''), 3000); load(); };

  const data = tab === 'whitelist' ? wl : bl;
  const isWL = tab === 'whitelist';

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
      {toast && <div className="anim-slide-r" style={{ position: 'fixed', top: '20px', right: '20px', zIndex: 300, background: 'rgba(0,255,153,.1)', border: '1.5px solid rgba(0,255,153,.4)', color: 'var(--green)', fontFamily: 'Orbitron, monospace', fontSize: '11px', letterSpacing: '.1em', padding: '12px 18px', boxShadow: 'var(--glow-g)' }}>{toast.toUpperCase()}</div>}

      <div>
        <div className="label-xs" style={{ marginBottom: '6px' }}>MODULE // PLY-001</div>
        <h1 style={{ fontFamily: 'Orbitron, monospace', fontSize: '28px', fontWeight: '800', color: 'var(--text)', letterSpacing: '.05em' }}>⛨ Policy Engine</h1>
      </div>

      <div style={{ background: isWL ? 'rgba(0,255,153,.06)' : 'rgba(255,34,85,.06)', border: '1.5px solid ' + (isWL ? 'rgba(0,255,153,.25)' : 'rgba(255,34,85,.25)'), padding: '16px 20px', display: 'flex', gap: '14px', alignItems: 'flex-start' }}>
        <span style={{ fontSize: '26px' }}>{isWL ? '✅' : '🚫'}</span>
        <div>
          <div style={{ fontFamily: 'Orbitron, monospace', fontSize: '13px', fontWeight: '600', color: isWL ? 'var(--green)' : 'var(--red)', letterSpacing: '.1em', marginBottom: '5px' }}>
            {isWL ? 'WHITELIST POLICY ACTIVE' : 'BLOCK POLICY ACTIVE'}
          </div>
          <div style={{ fontFamily: 'Exo 2, sans-serif', fontSize: '14px', color: 'var(--dim)', lineHeight: '1.5' }}>
            {isWL
              ? 'Whitelisted devices are classified as SAFE. No alerts will be generated when they connect to any monitored endpoint.'
              : 'Blocked devices trigger CRITICAL risk classification and immediate alerts when connected to any monitored endpoint.'}
          </div>
        </div>
      </div>

      <div style={{ display: 'flex', gap: '6px' }}>
        {[
          { k: 'whitelist', emoji: '✅', label: 'WHITELIST [' + wl.length + ']', col: 'var(--green)' },
          { k: 'blocked',   emoji: '🚫', label: 'BLOCKED ['   + bl.length + ']', col: 'var(--red)'   },
        ].map(t => (
          <button key={t.k} onClick={() => setTab(t.k)} className="btn" style={{
            color: tab === t.k ? t.col : 'var(--dim)',
            borderColor: tab === t.k ? t.col + '60' : 'var(--border)',
            background: tab === t.k ? t.col + '12' : 'transparent',
            fontSize: '12px', padding: '10px 22px',
          }}>{t.emoji} {t.label}</button>
        ))}
      </div>

      <div className={'panel ' + (isWL ? 'panel-green' : 'panel-red')} style={{ overflow: 'hidden' }}>
        {loading ? (
          <div style={{ textAlign: 'center', padding: '60px', color: 'var(--muted)', fontFamily: 'Share Tech Mono, monospace' }}>🔄 LOADING POLICY RULES...</div>
        ) : !data.length ? (
          <div style={{ textAlign: 'center', padding: '60px', color: 'var(--muted)', fontFamily: 'Share Tech Mono, monospace' }}>
            <div style={{ fontSize: '40px', marginBottom: '14px' }}>{isWL ? '📋' : '🚫'}</div>
            NO {tab.toUpperCase()} RULES — USE DEVICE MONITOR → ACTIONS TO ADD
          </div>
        ) : (
          <div style={{ overflowX: 'auto' }}>
            <table className="tbl">
              <thead><tr>{['#', 'Device Name', 'VID', 'PID', 'Serial', isWL ? 'Notes' : 'Reason', 'Date', 'Action'].map(h => <th key={h}>{h}</th>)}</tr></thead>
              <tbody>
                {data.map((d, i) => (
                  <tr key={d.id}>
                    <td style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '11px', color: 'var(--muted)' }}>{String(i + 1).padStart(3, '0')}</td>
                    <td><div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}><span style={{ fontSize: '15px' }}>💾</span><span style={{ fontSize: '14px', fontWeight: '600', color: 'var(--text)' }}>{d.device_name || '—'}</span></div></td>
                    <td style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--cyan)', fontWeight: '600' }}>{d.vendor_id || '—'}</td>
                    <td style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--dim)' }}>{d.product_id || '—'}</td>
                    <td style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '11px', color: 'var(--dim)', maxWidth: '120px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{d.serial_number || '—'}</td>
                    <td style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '11px', color: 'var(--dim)', maxWidth: '180px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{(isWL ? d.notes : d.reason) || '—'}</td>
                    <td style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '10px', color: 'var(--muted)', whiteSpace: 'nowrap' }}>{d.created_at ? new Date(d.created_at).toLocaleDateString() : '—'}</td>
                    <td>
                      {isWL
                        ? <button className="btn btn-r" style={{ padding: '6px 12px', fontSize: '10px' }} onClick={() => remove(d.id)}>🗑️ REMOVE</button>
                        : <button className="btn btn-g" style={{ padding: '6px 12px', fontSize: '10px' }} onClick={() => unblock(d.id)}>♻️ UNBLOCK</button>
                      }
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </div>
  );
}
