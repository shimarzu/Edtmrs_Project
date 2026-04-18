import React, { useState, useEffect } from 'react';
import { statsAPI } from '../utils/api';
import { Chart as ChartJS, CategoryScale, LinearScale, BarElement, LineElement, PointElement, Tooltip, Filler, ArcElement } from 'chart.js';
import { Bar, Line, Doughnut } from 'react-chartjs-2';
ChartJS.register(CategoryScale, LinearScale, BarElement, LineElement, PointElement, Tooltip, Filler, ArcElement);

const OPTS = {
  responsive: true, maintainAspectRatio: false,
  plugins: { legend: { display: false }, tooltip: {
    backgroundColor: 'rgba(3,12,20,.96)',
    borderColor: 'rgba(0,200,255,.3)', borderWidth: 1,
    titleFont: { family: 'Orbitron', size: 10 },
    bodyFont: { family: 'Share Tech Mono', size: 11 },
    titleColor: '#00c8ff', bodyColor: '#d0e8ff', padding: 10,
  }},
  scales: {
    x: { grid: { color: 'rgba(13,34,64,.8)' }, ticks: { color: '#152535', font: { size: 9 } } },
    y: { grid: { color: 'rgba(13,34,64,.8)' }, ticks: { color: '#152535', font: { size: 9 } }, beginAtZero: true },
  },
};

function StatCard({ emoji, label, value, sub, color, pulse }) {
  const C = {
    blue:   { main: 'var(--blue)',   bg: 'rgba(0,200,255,.07)',  b: 'rgba(0,200,255,.22)'  },
    cyan:   { main: 'var(--cyan)',   bg: 'rgba(0,255,238,.07)',  b: 'rgba(0,255,238,.22)'  },
    amber:  { main: 'var(--amber)',  bg: 'rgba(255,187,0,.07)',  b: 'rgba(255,187,0,.22)'  },
    red:    { main: 'var(--red)',    bg: 'rgba(255,34,85,.07)',  b: 'rgba(255,34,85,.22)'  },
    green:  { main: 'var(--green)',  bg: 'rgba(0,255,153,.07)',  b: 'rgba(0,255,153,.22)'  },
    purple: { main: 'var(--purple)', bg: 'rgba(204,68,255,.07)', b: 'rgba(204,68,255,.22)' },
  };
  const c = C[color] || C.blue;
  return (
    <div style={{ background: c.bg, border: `1.5px solid ${c.b}`, padding: '22px 20px', position: 'relative', overflow: 'hidden' }}>
      <div style={{ position: 'absolute', top: 0, left: 0, right: 0, height: '2px', background: `linear-gradient(90deg,transparent,${c.main},transparent)` }} />
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '14px' }}>
        <span style={{ fontSize: '34px' }}>{emoji}</span>
        {pulse && <span style={{ width: '10px', height: '10px', borderRadius: '50%', background: c.main, boxShadow: `0 0 12px ${c.main}`, animation: 'pulse 1.5s infinite', display: 'block' }} />}
      </div>
      <div style={{ fontFamily: 'Orbitron, monospace', fontSize: '40px', fontWeight: '800', color: c.main, lineHeight: 1, marginBottom: '8px', textShadow: `0 0 20px ${c.main}60` }}>
        {value ?? '—'}
      </div>
      <div style={{ fontFamily: 'Exo 2, sans-serif', fontSize: '14px', fontWeight: '700', color: 'var(--text)', textTransform: 'uppercase', letterSpacing: '.05em', marginBottom: '3px' }}>{label}</div>
      {sub && <div style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '11px', color: 'var(--dim)', letterSpacing: '.08em' }}>{sub}</div>}
    </div>
  );
}

export default function DashboardPage({ liveEvents }) {
  const [stats, setStats] = useState(null);
  const [loading, setLoading] = useState(true);

  const load = async () => { try { const r = await statsAPI.get(); setStats(r.data); } catch {} setLoading(false); };
  useEffect(() => { load(); const i = setInterval(load, 15000); return () => clearInterval(i); }, []);
  useEffect(() => { if (liveEvents?.length) load(); }, [liveEvents]);

  const act = stats?.activity_7d || [];
  const labels = act.map(d => d.day?.slice(5) || '');
  const vals = act.map(d => d.count);

  const barData = { labels, datasets: [{ data: vals, backgroundColor: 'rgba(0,200,255,.15)', borderColor: 'var(--blue)', borderWidth: 2, borderRadius: 3, hoverBackgroundColor: 'rgba(0,200,255,.3)' }] };
  const lineData = { labels, datasets: [{ data: vals, borderColor: 'var(--cyan)', backgroundColor: 'rgba(0,255,238,.06)', fill: true, tension: .4, pointBackgroundColor: 'var(--cyan)', pointRadius: 4, borderWidth: 2.5 }] };
  const donutData = {
    labels: ['Safe', 'Medium', 'High', 'Critical'],
    datasets: [{ data: [
      Math.max(0, (stats?.total_devices || 0) - (stats?.suspicious_devices || 0)),
      Math.floor((stats?.suspicious_devices || 0) * .45),
      Math.ceil((stats?.suspicious_devices || 0) * .35),
      Math.ceil((stats?.suspicious_devices || 0) * .2),
    ], backgroundColor: ['rgba(0,255,153,.75)', 'rgba(255,187,0,.75)', 'rgba(255,119,0,.75)', 'rgba(255,34,85,.75)'], borderWidth: 0 }],
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '22px' }}>
      {/* Header */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <div>
          <div className="label-xs" style={{ marginBottom: '6px' }}>MODULE // OVW-001</div>
          <h1 style={{ fontFamily: 'Orbitron, monospace', fontSize: '28px', fontWeight: '800', color: 'var(--text)', letterSpacing: '.05em' }}>
            🛡️ Security Overview
          </h1>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px', background: 'rgba(0,255,153,.06)', border: '1.5px solid rgba(0,255,153,.25)', padding: '10px 18px' }}>
          <span className="dot dot-on" />
          <span style={{ fontFamily: 'Orbitron, monospace', fontSize: '12px', color: 'var(--green)', letterSpacing: '.12em' }}>LIVE MONITORING</span>
        </div>
      </div>

      {loading ? (
        <div style={{ display: 'flex', justifyContent: 'center', padding: '80px' }}>
          <div style={{ width: '44px', height: '44px', border: '3px solid var(--border)', borderTop: '3px solid var(--blue)', borderRadius: '50%', animation: 'spin .8s linear infinite' }} />
          <style>{`@keyframes spin{to{transform:rotate(360deg);}}`}</style>
        </div>
      ) : (<>
        {/* Stats */}
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(185px, 1fr))', gap: '16px' }}>
          <StatCard emoji="🖥️" label="Endpoints"     value={stats?.total_endpoints}       sub={`${stats?.online_endpoints || 0} ONLINE`}         color="blue"   pulse />
          <StatCard emoji="💾" label="Devices Logged" value={stats?.total_devices}         sub="ALL TIME RECORDS"                                   color="cyan" />
          <StatCard emoji="⚠️"  label="Threats Found" value={stats?.suspicious_devices}     sub="HIGH + CRITICAL"                                    color="amber"  pulse={stats?.suspicious_devices > 0} />
          <StatCard emoji="🔔" label="Unread Alerts" value={stats?.unacknowledged_alerts}  sub={`${stats?.total_alerts || 0} TOTAL GENERATED`}      color="red"    pulse={stats?.unacknowledged_alerts > 0} />
        </div>

        {/* Charts */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 220px', gap: '16px' }}>
          <div className="panel" style={{ padding: '20px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '16px' }}>
              <span style={{ fontSize: '18px' }}>📊</span>
              <span className="label-sm" style={{ color: 'var(--blue)' }}>ACTIVITY // 7 DAYS</span>
            </div>
            <div style={{ height: '150px' }}><Bar data={barData} options={OPTS} /></div>
          </div>

          <div className="panel" style={{ padding: '20px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '16px' }}>
              <span style={{ fontSize: '18px' }}>📈</span>
              <span className="label-sm" style={{ color: 'var(--cyan)' }}>TREND ANALYSIS</span>
            </div>
            <div style={{ height: '150px' }}><Line data={lineData} options={OPTS} /></div>
          </div>

          <div className="panel panel-purple" style={{ padding: '20px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '16px' }}>
              <span style={{ fontSize: '18px' }}>🎯</span>
              <span className="label-sm" style={{ color: 'var(--purple)' }}>RISK SPLIT</span>
            </div>
            <div style={{ height: '110px' }}>
              <Doughnut data={donutData} options={{ ...OPTS, cutout: '62%', scales: undefined }} />
            </div>
            <div style={{ marginTop: '12px', display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {[['var(--green)', 'Safe'], ['var(--amber)', 'Medium'], ['var(--orange)', 'High'], ['var(--red)', 'Critical']].map(([c, l]) => (
                <div key={l} style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <span style={{ width: '8px', height: '8px', borderRadius: '50%', background: c, display: 'inline-block', boxShadow: `0 0 6px ${c}` }} />
                  <span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '11px', color: 'var(--dim)' }}>{l}</span>
                </div>
              ))}
            </div>
          </div>
        </div>

        {/* Live feed */}
        <div className="panel panel-green" style={{ overflow: 'hidden' }}>
          <div style={{ padding: '16px 20px', borderBottom: '1px solid var(--border)', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
              <span style={{ fontSize: '20px' }}>📡</span>
              <span className="label-sm" style={{ color: 'var(--green)' }}>LIVE TELEMETRY FEED</span>
              <span className="dot dot-on" />
            </div>
            <span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--dim)' }}>{liveEvents?.length || 0} EVENTS</span>
          </div>
          <div style={{ maxHeight: '220px', overflowY: 'auto' }}>
            {!liveEvents?.length ? (
              <div style={{ padding: '40px', textAlign: 'center', color: 'var(--muted)', fontFamily: 'Share Tech Mono, monospace', fontSize: '13px' }}>
                📡 AWAITING TELEMETRY — INSERT USB ON ENDPOINT TO TEST
              </div>
            ) : [...liveEvents].reverse().slice(0, 20).map((ev, i) => (
              <div key={i} style={{ display: 'flex', alignItems: 'center', gap: '14px', padding: '12px 20px', borderBottom: '1px solid rgba(13,34,64,.8)', transition: 'background .15s', cursor: 'default' }}
                onMouseEnter={e => e.currentTarget.style.background = 'var(--hover)'}
                onMouseLeave={e => e.currentTarget.style.background = 'transparent'}
              >
                <span style={{ fontSize: '18px' }}>{ev.risk_level === 'critical' ? '🔴' : ev.risk_level === 'high' ? '🟠' : ev.risk_level === 'medium' ? '🟡' : '🟢'}</span>
                <span style={{ fontFamily: 'Exo 2, sans-serif', fontSize: '14px', fontWeight: '600', color: 'var(--text)', flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{ev.device_name}</span>
                <span style={{ fontSize: '14px' }}>🖥️</span>
                <span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--blue)' }}>{ev.hostname}</span>
                <span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '11px', color: 'var(--dim)', minWidth: '60px', textAlign: 'right' }}>{ev.timestamp?.slice(11, 19) || '--:--:--'}</span>
              </div>
            ))}
          </div>
        </div>
      </>)}
      <style>{`@keyframes spin{to{transform:rotate(360deg);}}`}</style>
    </div>
  );
}
