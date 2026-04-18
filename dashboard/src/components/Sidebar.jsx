import React, { useState, useEffect } from 'react';
import { NavLink } from 'react-router-dom';
import { useAuth } from '../hooks/useAuth';

const NAV = [
  { path:'/',          icon:'⬡', label:'OVERVIEW',       sub:'Security Dashboard'    },
  { path:'/devices',   icon:'◈', label:'DEVICE MONITOR', sub:'USB Threat Detection'  },
  { path:'/endpoints', icon:'⬢', label:'ENDPOINTS',      sub:'Network Nodes'         },
  { path:'/alerts',    icon:'⚠',  label:'THREAT ALERTS',  sub:'Active Incidents'      },
  { path:'/policy',    icon:'⛨', label:'POLICY ENGINE',  sub:'Rules & Whitelist'     },
];

export default function Sidebar({ alertCount = 0 }) {
  const { user, logout } = useAuth();
  const [time, setTime]  = useState(new Date());
  const [secs, setSecs]  = useState(0);

  useEffect(() => {
    const t = setInterval(() => { setTime(new Date()); setSecs(s => s + 1); }, 1000);
    return () => clearInterval(t);
  }, []);

  const pad = n => String(n).padStart(2, '0');
  const uptime = `${pad(Math.floor(secs/3600))}:${pad(Math.floor((secs%3600)/60))}:${pad(secs%60)}`;

  return (
    <aside style={{
      position:'fixed', left:0, top:0, bottom:0, width:'240px',
      background:'linear-gradient(180deg, #030c14 0%, #020810 100%)',
      borderRight:'1px solid var(--border)',
      display:'flex', flexDirection:'column', zIndex:40,
    }}>
      {/* Rainbow top bar */}
      <div style={{ height:'3px', background:'linear-gradient(90deg, var(--blue), var(--cyan), var(--purple), var(--pink))' }} />

      {/* Logo */}
      <div style={{ padding:'22px 20px 16px', borderBottom:'1px solid var(--border)' }}>
        <div style={{ display:'flex', alignItems:'center', gap:'13px', marginBottom:'12px' }}>
          <div style={{
            width:'44px', height:'44px', flexShrink:0,
            background:'rgba(0,200,255,.08)',
            border:'2px solid rgba(0,200,255,.3)',
            display:'flex', alignItems:'center', justifyContent:'center',
            fontSize:'22px', color:'var(--blue)',
            boxShadow:'0 0 20px rgba(0,200,255,.2)',
          }}>⬡</div>
          <div>
            <div style={{
              fontFamily:"'Orbitron', sans-serif", fontSize:'20px', fontWeight:'900',
              color:'var(--blue)', letterSpacing:'.08em',
              textShadow:'0 0 20px rgba(0,200,255,.6)',
            }} className="anim-flicker">EDTMRS</div>
            <div style={{ fontFamily:"'Share Tech Mono',monospace", fontSize:'10px', color:'var(--dim)', letterSpacing:'.15em', marginTop:'2px' }}>
              v6.0 · USB BLOCKING
            </div>
          </div>
        </div>
        {/* Clock */}
        <div style={{
          fontFamily:"'Share Tech Mono',monospace", fontSize:'13px',
          color:'var(--cyan)', letterSpacing:'.06em',
          background:'rgba(0,255,238,.05)', border:'1px solid rgba(0,255,238,.15)',
          padding:'7px 12px', display:'flex', justifyContent:'space-between',
        }}>
          <span>{time.toTimeString().slice(0,8)}</span>
          <span style={{ color:'var(--dim)', fontSize:'10px' }}>{time.toDateString().slice(4)}</span>
        </div>
      </div>

      {/* System vitals */}
      <div style={{ padding:'12px 20px', borderBottom:'1px solid var(--border)' }}>
        <div className="label-xs" style={{ color:'var(--blue)', marginBottom:'10px' }}>◈ SYSTEM VITALS</div>
        {[
          { k:'STATUS', v:'ACTIVE',                                        color:'var(--green)', dot:'dot-on'   },
          { k:'UPTIME', v:uptime,                                          color:'var(--blue)',  dot:null       },
          { k:'STREAM', v:'CONNECTED',                                     color:'var(--cyan)',  dot:'dot-on'   },
          { k:'THREAT', v:alertCount > 0 ? `${alertCount} ACTIVE`:'CLEAR', color:alertCount>0?'var(--red)':'var(--green)', dot:alertCount>0?'dot-crit':'dot-on' },
        ].map(s => (
          <div key={s.k} style={{ display:'flex', justifyContent:'space-between', alignItems:'center', marginBottom:'7px' }}>
            <span className="label-xs">{s.k}</span>
            <div style={{ display:'flex', alignItems:'center', gap:'7px' }}>
              {s.dot && <span className={`dot ${s.dot}`} />}
              <span style={{ fontFamily:"'Share Tech Mono',monospace", fontSize:'12px', color:s.color }}>{s.v}</span>
            </div>
          </div>
        ))}
      </div>

      {/* Navigation */}
      <nav style={{ flex:1, padding:'10px 12px', overflowY:'auto' }}>
        <div className="label-xs" style={{ padding:'6px 8px', marginBottom:'6px' }}>MODULES</div>
        {NAV.map(item => (
          <NavLink key={item.path} to={item.path} end={item.path === '/'}
            style={({ isActive }) => ({
              display:'flex', alignItems:'center', gap:'12px',
              padding:'11px 12px', marginBottom:'3px',
              textDecoration:'none',
              background: isActive ? 'rgba(0,200,255,.09)' : 'transparent',
              borderLeft: isActive ? '3px solid var(--blue)' : '3px solid transparent',
              transition:'all .15s',
            })}
          >
            {({ isActive }) => (<>
              <span style={{
                fontSize:'18px', width:'22px', textAlign:'center',
                color: isActive ? 'var(--blue)' : 'var(--dim)',
                filter: isActive ? 'drop-shadow(0 0 6px var(--blue))' : 'none',
              }}>{item.icon}</span>
              <div style={{ flex:1 }}>
                <div style={{
                  fontFamily:"'Orbitron',sans-serif", fontSize:'11px', fontWeight:'600',
                  color: isActive ? 'var(--text)' : 'var(--dim)',
                  letterSpacing:'.08em', marginBottom:'2px',
                }}>{item.label}</div>
                <div style={{
                  fontFamily:"'Share Tech Mono',monospace", fontSize:'10px',
                  color: isActive ? 'var(--blue)' : 'var(--muted)', letterSpacing:'.06em',
                }}>{item.sub}</div>
              </div>
              {item.label === 'THREAT ALERTS' && alertCount > 0 && (
                <span style={{
                  fontFamily:"'Orbitron',sans-serif", fontSize:'10px', fontWeight:'700',
                  background:'var(--red)', color:'#fff', padding:'3px 7px',
                  boxShadow:'0 0 12px rgba(255,34,85,.6)',
                  animation:'pulse 1s infinite',
                }}>{alertCount > 99 ? '99+' : alertCount}</span>
              )}
            </>)}
          </NavLink>
        ))}
      </nav>

      {/* User */}
      <div style={{ padding:'14px 20px', borderTop:'1px solid var(--border)' }}>
        <div style={{ display:'flex', alignItems:'center', gap:'12px', marginBottom:'10px' }}>
          <div style={{
            width:'36px', height:'36px', flexShrink:0,
            background:'rgba(0,200,255,.1)', border:'2px solid rgba(0,200,255,.3)',
            display:'flex', alignItems:'center', justifyContent:'center',
            fontFamily:"'Orbitron',sans-serif", fontSize:'14px', fontWeight:'700', color:'var(--blue)',
          }}>{user?.username?.[0]?.toUpperCase() || 'A'}</div>
          <div>
            <div style={{ fontFamily:"'Exo 2',sans-serif", fontSize:'14px', fontWeight:'600', color:'var(--text)' }}>{user?.username}</div>
            <div style={{ fontFamily:"'Share Tech Mono',monospace", fontSize:'10px', color:'var(--cyan)', letterSpacing:'.1em', textTransform:'uppercase' }}>{user?.role}</div>
          </div>
        </div>
        <button onClick={logout} className="btn btn-r" style={{ width:'100%', justifyContent:'center', padding:'9px', fontSize:'10px' }}>
          ⏻ TERMINATE SESSION
        </button>
      </div>
    </aside>
  );
}
