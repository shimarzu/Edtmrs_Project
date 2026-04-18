import React, { useState, useEffect, useCallback } from 'react';
import { devicesAPI, actionsAPI, exportAPI, downloadCSV } from '../utils/api';
import RiskBadge from '../components/RiskBadge';

const FILTERS = ['ALL', 'CRITICAL', 'HIGH', 'MEDIUM', 'SAFE', 'UNKNOWN'];

function Modal({ device, onClose, onDone }) {
  const [reason, setReason] = useState('');
  const [loading, setLoading] = useState(false);
  const doBlock = async () => {
    setLoading(true);
    try { await actionsAPI.blockDevice({ vendor_id: device.vendor_id, product_id: device.product_id, serial_number: device.serial_number, device_name: device.device_name, reason: reason || 'Blocked by admin' }); onDone('blocked'); }
    finally { setLoading(false); }
  };
  const doWL = async () => {
    setLoading(true);
    try { await actionsAPI.whitelistDevice({ vendor_id: device.vendor_id, product_id: device.product_id, serial_number: device.serial_number, device_name: device.device_name, notes: reason }); onDone('whitelisted'); }
    finally { setLoading(false); }
  };
  return (
    <div style={{ position: 'fixed', inset: 0, background: 'rgba(1,5,8,.88)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 200, backdropFilter: 'blur(6px)' }}>
      <div className="panel corners anim-slide-u" style={{ width: '440px', overflow: 'hidden' }}>
        <div style={{ padding: '18px 22px', borderBottom: '1px solid var(--border)', display: 'flex', justifyContent: 'space-between', alignItems: 'center', background: 'rgba(0,200,255,.04)' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <span style={{ fontSize: '20px' }}>⚙️</span>
            <span className="label-sm" style={{ color: 'var(--blue)' }}>DEVICE ACTION CONSOLE</span>
          </div>
          <button onClick={onClose} style={{ background: 'none', border: 'none', color: 'var(--dim)', cursor: 'pointer', fontSize: '20px', lineHeight: 1 }}>✕</button>
        </div>
        <div style={{ padding: '20px 22px' }}>
          <div style={{ background: 'var(--deep)', border: '1px solid var(--border)', padding: '16px', marginBottom: '18px' }}>
            {[['💾', 'DEVICE', device.device_name], ['🔌', 'VID', device.vendor_id], ['📦', 'PID', device.product_id], ['🔢', 'SERIAL', device.serial_number], ['🖥️', 'HOST', device.hostname], ['👤', 'USER', device.username]].map(([ico, k, v]) => (
              <div key={k} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <span style={{ fontSize: '14px' }}>{ico}</span>
                  <span className="label-xs">{k}</span>
                </div>
                <span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--text)', maxWidth: '55%', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{v || '—'}</span>
              </div>
            ))}
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', paddingTop: '10px', borderTop: '1px solid var(--border)', marginTop: '4px' }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ fontSize: '14px' }}>⚠️</span>
                <span className="label-xs">RISK LEVEL</span>
              </div>
              <RiskBadge level={device.risk_level} />
            </div>
          </div>
          <div style={{ marginBottom: '18px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px' }}>
              <span style={{ fontSize: '14px' }}>📝</span>
              <span className="label-xs">REASON / NOTES</span>
            </div>
            <input className="inp" value={reason} onChange={e => setReason(e.target.value)} placeholder="Enter reason for this action..." />
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            <button className="btn btn-g" style={{ justifyContent: 'center', padding: '12px' }} onClick={doWL} disabled={loading}>✅ WHITELIST</button>
            <button className="btn btn-r" style={{ justifyContent: 'center', padding: '12px' }} onClick={doBlock} disabled={loading}>🚫 BLOCK</button>
          </div>
        </div>
      </div>
    </div>
  );
}

export default function DevicesPage({ liveEvents }) {
  const [devices, setDevices] = useState([]);
  const [total, setTotal] = useState(0);
  const [loading, setLoading] = useState(true);
  const [search, setSearch] = useState('');
  const [risk, setRisk] = useState('ALL');
  const [selected, setSelected] = useState(null);
  const [toast, setToast] = useState('');

  const load = useCallback(async () => {
    try { const r = await devicesAPI.list({ search: search || undefined, risk_level: risk === 'ALL' ? undefined : risk.toLowerCase(), limit: 100 }); setDevices(r.data.devices); setTotal(r.data.total); }
    catch {} setLoading(false);
  }, [search, risk]);

  useEffect(() => { load(); }, [load]);
  useEffect(() => { if (liveEvents?.length) load(); }, [liveEvents, load]);

  const handleDone = action => { setSelected(null); setToast('Device ' + action + ' successfully'); setTimeout(() => setToast(''), 3000); load(); };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
      {toast && <div className="anim-slide-r" style={{ position: 'fixed', top: '20px', right: '20px', zIndex: 300, background: 'rgba(0,255,153,.1)', border: '1.5px solid rgba(0,255,153,.4)', color: 'var(--green)', fontFamily: 'Orbitron, monospace', fontSize: '11px', letterSpacing: '.1em', padding: '12px 18px', boxShadow: 'var(--glow-g)' }}>✅ {toast.toUpperCase()}</div>}
      {selected && <Modal device={selected} onClose={() => setSelected(null)} onDone={handleDone} />}

      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <div>
          <div className="label-xs" style={{ marginBottom: '6px' }}>MODULE // DVM-001</div>
          <h1 style={{ fontFamily: 'Orbitron, monospace', fontSize: '28px', fontWeight: '800', color: 'var(--text)', letterSpacing: '.05em' }}>💾 Device Monitor</h1>
        </div>
        <div style={{ display:'flex', alignItems:'center', gap:'12px' }}>
          <span style={{ fontFamily:"'Share Tech Mono',monospace", fontSize:'13px', color:'var(--dim)' }}>{total} RECORDS</span>
          <button className="btn btn-g" style={{ padding:'8px 16px', fontSize:'10px' }}
            onClick={async () => { try { const r = await exportAPI.devices(); downloadCSV(r.data,'edtmrs_devices.csv'); } catch(e){} }}>
            📥 EXPORT CSV
          </button>
        </div>
      </div>

      <div style={{ display: 'flex', gap: '12px', flexWrap: 'wrap', alignItems: 'center' }}>
        <input className="inp" style={{ width: '300px' }} value={search} onChange={e => setSearch(e.target.value)} placeholder="🔍  Search name, host, serial, VID..." />
        <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
          {FILTERS.map(f => (
            <button key={f} onClick={() => setRisk(f)} className={'btn ' + (risk === f ? 'btn-b' : '')} style={risk !== f ? { color: 'var(--dim)', borderColor: 'var(--border)', background: 'transparent' } : {}}>
              {f}
            </button>
          ))}
        </div>
      </div>

      <div className="panel" style={{ overflow: 'hidden' }}>
        <div style={{ overflowX: 'auto' }}>
          <table className="tbl">
            <thead><tr>{['#', 'Device Name', 'VID / PID', 'Serial', 'Drive', 'Endpoint', 'User', 'Risk', 'Time', 'Action'].map(h => <th key={h}>{h}</th>)}</tr></thead>
            <tbody>
              {loading ? (
                <tr><td colSpan={11} style={{ textAlign: 'center', padding: '60px', color: 'var(--muted)', fontFamily: 'Share Tech Mono, monospace' }}>🔄 LOADING...</td></tr>
              ) : !devices.length ? (
                <tr><td colSpan={11} style={{ textAlign: 'center', padding: '60px', color: 'var(--muted)', fontFamily: 'Share Tech Mono, monospace' }}>📭 NO RECORDS FOUND</td></tr>
              ) : devices.map((d, i) => (
                <tr key={d.id}>
                  <td style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '11px', color: 'var(--muted)' }}>{String(i + 1).padStart(3, '0')}</td>
                  <td>
                    <div style={{ fontWeight: '700', fontSize: '14px', color: 'var(--text)', maxWidth: '160px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{d.device_name}</div>
                    <div style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '10px', color: 'var(--dim)', marginTop: '2px' }}>{d.device_type}</div>
                  </td>
                  <td>
                    <div style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--cyan)', fontWeight: '600' }}>{d.vendor_id}</div>
                    <div style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '11px', color: 'var(--dim)' }}>{d.product_id}</div>
                  </td>
                  <td style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '11px', color: 'var(--dim)', maxWidth: '110px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{d.serial_number || '—'}</td>
                  <td style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '14px', color: 'var(--blue)', fontWeight: '600' }}>{d.drive_letter || '—'}</td>
                  <td>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      <span style={{ fontSize: '14px' }}>🖥️</span>
                      <span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--text)' }}>{d.hostname}</span>
                    </div>
                  </td>
                  <td>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      <span style={{ fontSize: '14px' }}>👤</span>
                      <span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--dim)' }}>{d.username}</span>
                    </div>
                  </td>
                  <td><RiskBadge level={d.risk_level} pulse /></td>
                  <td>
                    <span style={{
                      fontFamily:"'Orbitron',monospace", fontSize:'13px', fontWeight:'700',
                      color: (d.connect_count||1) > 5 ? 'var(--red)' : (d.connect_count||1) > 2 ? 'var(--amber)' : 'var(--green)',
                      background: (d.connect_count||1) > 5 ? 'rgba(255,34,85,.1)' : (d.connect_count||1) > 2 ? 'rgba(255,187,0,.1)' : 'rgba(0,255,153,.1)',
                      padding:'3px 8px',
                      border: `1px solid ${(d.connect_count||1) > 5 ? 'rgba(255,34,85,.3)' : (d.connect_count||1) > 2 ? 'rgba(255,187,0,.3)' : 'rgba(0,255,153,.3)'}`,
                    }}>{d.connect_count || 1}x</span>
                  </td>
                  <td style={{ fontFamily:"'Share Tech Mono',monospace", fontSize:'10px', color:'var(--muted)', whiteSpace:'nowrap' }}>
                    {(d.last_seen||d.timestamp) ? new Date(d.last_seen||d.timestamp).toLocaleString() : '—'}
                  </td>
                  <td><button className="btn btn-b" style={{ padding: '6px 14px', fontSize: '10px' }} onClick={() => setSelected(d)}>⚙️ ACTIONS</button></td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
}
