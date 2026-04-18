import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../hooks/useAuth';

const BOOT = [
  { text: 'INITIALIZING EDTMRS CORE ENGINE...', color: 'var(--blue)' },
  { text: 'LOADING THREAT CLASSIFICATION MODULE...', color: 'var(--cyan)' },
  { text: 'ESTABLISHING ENCRYPTED CHANNELS...', color: 'var(--purple)' },
  { text: 'MOUNTING ENDPOINT TELEMETRY NODES...', color: 'var(--blue)' },
  { text: 'VERIFYING CRYPTOGRAPHIC SIGNATURES...', color: 'var(--cyan)' },
  { text: 'ALL SYSTEMS NOMINAL — AUTHENTICATION REQUIRED.', color: 'var(--green)' },
];

export default function LoginPage() {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError]       = useState('');
  const [loading, setLoading]   = useState(false);
  const [lines, setLines]       = useState([]);
  const [booting, setBooting]   = useState(true);
  const { login } = useAuth();
  const navigate  = useNavigate();

  useEffect(() => {
    let i = 0;
    const t = setInterval(() => {
      if (i < BOOT.length && BOOT[i]) { setLines(p => [...p, BOOT[i]]); i++; }
      else { clearInterval(t); setTimeout(() => setBooting(false), 400); }
    }, 320);
    return () => clearInterval(t);
  }, []);

  const submit = async e => {
    e.preventDefault(); setError(''); setLoading(true);
    try { await login(username, password); navigate('/'); }
    catch (err) { setError(err.response?.data?.detail || 'AUTHENTICATION FAILED — ACCESS DENIED'); }
    finally { setLoading(false); }
  };

  return (
    <div style={{ minHeight: '100vh', background: 'var(--void)', display: 'flex', alignItems: 'center', justifyContent: 'center', position: 'relative', overflow: 'hidden' }}>
      <div style={{ position: 'absolute', inset: 0, background: 'radial-gradient(ellipse 70% 60% at 50% 50%, rgba(0,200,255,.05) 0%, transparent 70%)' }} />
      <div style={{ position: 'absolute', top: '15%', left: '5%', width: '400px', height: '400px', background: 'radial-gradient(circle, rgba(0,200,255,.06) 0%, transparent 70%)', borderRadius: '50%' }} />
      <div style={{ position: 'absolute', bottom: '10%', right: '5%', width: '350px', height: '350px', background: 'radial-gradient(circle, rgba(204,68,255,.05) 0%, transparent 70%)', borderRadius: '50%' }} />
      <div style={{ position: 'absolute', inset: 0, backgroundImage: 'linear-gradient(rgba(0,200,255,.025) 1px, transparent 1px), linear-gradient(90deg, rgba(0,200,255,.025) 1px, transparent 1px)', backgroundSize: '50px 50px' }} />

      <div style={{ position: 'relative', width: '100%', maxWidth: '460px', padding: '0 24px' }}>
        {booting && (
          <div className="panel anim-fade" style={{ padding: '22px', marginBottom: '24px', background: 'rgba(0,0,0,.7)' }}>
            <div className="label-xs" style={{ color: 'var(--green)', marginBottom: '12px' }}>SYSTEM BOOTSTRAP SEQUENCE</div>
            {lines.filter(l => l && l.text).map((l, i) => (
              <div key={i} style={{ display: 'flex', alignItems: 'flex-start', gap: '10px', marginBottom: '6px' }}>
                <span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--dim)', flexShrink: 0 }}>[{String(i + 1).padStart(2, '0')}]</span>
                <span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: l?.color || 'var(--blue)', lineHeight: '1.4' }}>{l?.text || ''}</span>
              </div>
            ))}
            {lines.length > 0 && (
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginTop: '4px' }}>
                <span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--blue)' }}>&gt;</span>
                <span style={{ width: '10px', height: '16px', background: 'var(--blue)', animation: 'blink .9s infinite' }} />
              </div>
            )}
          </div>
        )}

        {!booting && (
          <div className="anim-slide-u">
            <div style={{ textAlign: 'center', marginBottom: '36px' }}>
              <div style={{ fontFamily: 'Orbitron, monospace', fontSize: '56px', fontWeight: '900', color: 'var(--blue)', letterSpacing: '.1em', textShadow: '0 0 40px rgba(0,200,255,.7), 0 0 80px rgba(0,200,255,.3)', marginBottom: '8px', lineHeight: 1 }} className="anim-flicker">
                EDTMRS
              </div>
              <div style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--dim)', letterSpacing: '.2em' }}>
                EXTERNAL DEVICE THREAT MONITORING SYSTEM
              </div>
              <div style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '11px', color: 'var(--muted)', letterSpacing: '.15em', marginTop: '4px' }}>
                SLIIT · CYBERSECURITY · 2026
              </div>
            </div>

            <div className="panel corners" style={{ padding: '32px' }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '24px' }}>
                <span style={{ fontSize: '22px' }}>🔐</span>
                <span className="label-sm" style={{ color: 'var(--blue)', fontSize: '12px' }}>OPERATOR AUTHENTICATION</span>
              </div>

              {error && (
                <div style={{ background: 'rgba(255,34,85,.1)', border: '1.5px solid rgba(255,34,85,.4)', color: 'var(--red)', fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', padding: '12px 14px', marginBottom: '18px', display: 'flex', alignItems: 'center', gap: '10px' }}>
                  <span style={{ fontSize: '16px' }}>⛔</span> {error}
                </div>
              )}

              <form onSubmit={submit}>
                <div style={{ marginBottom: '16px' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px' }}>
                    <span style={{ fontSize: '16px' }}>👤</span>
                    <span className="label-xs">OPERATOR ID</span>
                  </div>
                  <input className="inp" type="text" value={username} onChange={e => setUsername(e.target.value)} placeholder="Enter username" required />
                </div>
                <div style={{ marginBottom: '24px' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px' }}>
                    <span style={{ fontSize: '16px' }}>🔑</span>
                    <span className="label-xs">ACCESS KEY</span>
                  </div>
                  <input className="inp" type="password" value={password} onChange={e => setPassword(e.target.value)} placeholder="Enter password" required />
                </div>
                <button type="submit" disabled={loading} className="btn btn-b" style={{ width: '100%', justifyContent: 'center', padding: '14px', fontSize: '13px', fontWeight: '700', boxShadow: loading ? 'none' : 'var(--glow-b)', letterSpacing: '.15em' }}
                  onMouseEnter={e => { if (!loading) e.currentTarget.style.background = 'rgba(0,200,255,.14)'; }}
                  onMouseLeave={e => { e.currentTarget.style.background = 'rgba(0,200,255,.06)'; }}
                >
                  {loading
                    ? <><span style={{ width: '16px', height: '16px', border: '2px solid rgba(0,200,255,.3)', borderTop: '2px solid var(--blue)', borderRadius: '50%', animation: 'spin .8s linear infinite', display: 'inline-block', marginRight: '10px' }} />AUTHENTICATING...</>
                    : <><span style={{ fontSize: '18px', marginRight: '8px' }}>🚀</span>AUTHENTICATE & ENTER</>
                  }
                </button>
              </form>

              <div style={{ borderTop: '1px solid var(--border)', marginTop: '22px', paddingTop: '16px', textAlign: 'center' }}>
                <span style={{ fontFamily: 'Share Tech Mono, monospace', fontSize: '12px', color: 'var(--muted)' }}>
                  DEFAULT: admin  ·  Admin@1234
                </span>
              </div>
            </div>
          </div>
        )}
      </div>
      <style>{`@keyframes spin{to{transform:rotate(360deg);}}`}</style>
    </div>
  );
}
