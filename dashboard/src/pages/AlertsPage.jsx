import React, { useState, useEffect, useCallback } from 'react';
import { alertsAPI, exportAPI, downloadCSV } from '../utils/api';
import RiskBadge from '../components/RiskBadge';

export default function AlertsPage({ liveEvents, onAlertCountChange }) {
  const [alerts, setAlerts] = useState([]);
  const [loading, setLoading] = useState(true);
  const [filter, setFilter] = useState('unread');
  const [toast, setToast] = useState('');

  const load = useCallback(async () => {
    try {
      const p = {};
      if (filter === 'unread') p.acknowledged = false;
      if (filter === 'read')   p.acknowledged = true;
      const r = await alertsAPI.list(p);
      setAlerts(r.data.alerts);
      if (filter !== 'read') onAlertCountChange?.(r.data.alerts.filter(a => !a.is_acknowledged).length);
    } catch {} setLoading(false);
  }, [filter, onAlertCountChange]);

  useEffect(() => { load(); }, [load]);
  useEffect(() => { if (liveEvents?.length) load(); }, [liveEvents, load]);

  const ack    = async id => { await alertsAPI.acknowledge(id); load(); };
  const ackAll = async () => { await alertsAPI.acknowledgeAll(); setToast('✅ ALL ALERTS CLEARED'); setTimeout(() => setToast(''), 3000); load(); };
  const unread = alerts.filter(a => !a.is_acknowledged).length;

  const sevEmoji = { critical: '🔴', high: '🟠', medium: '🟡', low: '🔵' };
  const sevColor = { critical: 'var(--red)', high: 'var(--orange)', medium: 'var(--amber)', low: 'var(--blue)' };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
      {toast && <div className="anim-slide-r" style={{ position: 'fixed', top: '20px', right: '20px', zIndex: 300, background: 'rgba(0,255,153,.1)', border: '1.5px solid rgba(0,255,153,.4)', color: 'var(--green)', fontFamily: 'Orbitron, monospace', fontSize: '11px', letterSpacing: '.1em', padding: '12px 18px', boxShadow: 'var(--glow-g)' }}>{toast}</div>}

      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <div>
          <div className="label-xs" style={{ marginBottom: '6px' }}>MODULE // ALT-001</div>
          <h1 style={{ fontFamily: 'Orbitron, monospace', fontSize: '28px', fontWeight: '800', color: 'var(--text)', letterSpacing: '.05em' }}>⚠️ Threat Alerts</h1>
        </div>
        <div style={{ display:'flex', gap:'10px', alignItems:'center' }}>
          <button className="btn btn-g" style={{ fontSize:'10px', padding:'8px 16px' }}
            onClick={async () => { try { const r = await exportAPI.alerts(); downloadCSV(r.data,'edtmrs_alerts.csv'); } catch(e){} }}>
            📥 EXPORT CSV
          </button>
          {unread > 0 && <button className="btn btn-b" style={{ fontSize:'11px' }} onClick={ackAll}>✅ CLEAR ALL ({unread})</button>}
        </div>
      </div>

      <div style={{ display: 'flex', gap: '6px' }}>
        {[
          { k: 'unread', label: '🔔 UNREAD' + (unread ? ' [' + unread + ']' : '') },
          { k: 'read',   label: '✅ ACKNOWLEDGED' },
          { k: 'all',    label: '📋 ALL' },
        ].map(t => (
          <button key={t.k} onClick={() => setFilter(t.k)} className={'btn ' + (filter === t.k ? 'btn-b' : '')}
            style={filter !== t.k ? { color: 'var(--dim)', borderColor: 'var(--border)', background: 'transparent' } : {}}
          >{t.label}</button>
        ))}
      </div>

      {loading ? (
        <div style={{ textAlign: 'center', padding: '60px', color: 'var(--muted)', fontFamily: 'Share Tech Mono, monospace' }}>🔄 LOADING ALERTS...</div>
      ) : !alerts.length ? (
        <div className="panel" style={{ padding: '70px', textAlign: 'center' }}>
          <div style={{ fontSize: '56px', marginBottom: '16px' }}>✅</div>
          <div style={{ fontFamily: 'Orbitron, monospace', fontSize: '18px', fontWeight: '700', color: 'var(--green)', letterSpacing: '.15em', marginBottom: '8px' }}>NO ACTIVE THREATS</div>
          <div style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '13px', color: 'var(--dim)' }}>
            {filter === 'unread' ? 'All alerts acknowledged — system is clean' : 'No alerts in this category'}
          </div>
        </div>
      ) : (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
          {alerts.map(a => {
            const col = sevColor[a.severity] || sevColor.medium;
            const emo = sevEmoji[a.severity] || '🟡';
            return (
              <div key={a.id} className="anim-slide-u" style={{
                background: a.is_acknowledged ? 'var(--panel)' : 'var(--card)',
                border: '1.5px solid ' + (a.is_acknowledged ? 'var(--border)' : col + '45'),
                position: 'relative', overflow: 'hidden',
                opacity: a.is_acknowledged ? .6 : 1,
              }}>
                {!a.is_acknowledged && <div style={{ position: 'absolute', left: 0, top: 0, bottom: 0, width: '4px', background: col, boxShadow: '4px 0 12px ' + col + '60' }} />}
                <div style={{ padding: '18px 20px 18px 24px', display: 'flex', justifyContent: 'space-between', gap: '16px', alignItems: 'flex-start' }}>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '10px', flexWrap: 'wrap', marginBottom: '10px' }}>
                      <span style={{ fontSize: '22px' }}>{emo}</span>
                      <RiskBadge level={a.severity} pulse={!a.is_acknowledged} />
                      <span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '11px', color: 'var(--dim)' }}>ALERT #{a.id}</span>
                      {!a.is_acknowledged && <span style={{ fontFamily: 'Orbitron, monospace', fontSize: '9px', color: col, background: col + '18', border: '1px solid ' + col + '45', padding: '3px 8px', letterSpacing: '.1em', animation: 'pulse 1.5s infinite' }}>NEW</span>}
                    </div>
                    <div style={{ fontFamily: 'Exo 2, sans-serif', fontSize: '14px', color: a.is_acknowledged ? 'var(--dim)' : 'var(--text)', marginBottom: '12px', lineHeight: '1.6' }}>
                      {a.message}
                    </div>
                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '14px' }}>
                      {[['🖥️', 'HOST', a.hostname], ['👤', 'USER', a.username], ['💾', 'DEVICE', a.device_name], ['💿', 'DRIVE', a.drive_letter], ['🕐', 'TIME', a.created_at ? new Date(a.created_at).toLocaleString() : null]].filter(([,, v]) => v && v !== 'unknown').map(([ico, k, v]) => (
                        <div key={k} style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                          <span style={{ fontSize: '13px' }}>{ico}</span>
                          <span className="label-xs">{k}</span>
                          <span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '11px', color: 'var(--dim)' }}>{v}</span>
                        </div>
                      ))}
                    </div>
                  </div>
                  {!a.is_acknowledged && <button className="btn btn-b" style={{ padding: '8px 16px', fontSize: '10px', flexShrink: 0 }} onClick={() => ack(a.id)}>✅ ACK</button>}
                </div>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
}
