import React, { useState, useCallback } from 'react';
import { BrowserRouter, Routes, Route, Navigate, useNavigate } from 'react-router-dom';
import { AuthProvider, useAuth } from './hooks/useAuth';
import { useWebSocket } from './hooks/useWebSocket';
import Sidebar from './components/Sidebar';
import AlertPopup from './components/AlertPopup';
import LoginPage from './pages/LoginPage';
import DashboardPage from './pages/DashboardPage';
import DevicesPage from './pages/DevicesPage';
import EndpointsPage from './pages/EndpointsPage';
import AlertsPage from './pages/AlertsPage';
import PolicyPage from './pages/PolicyPage';


function AlertBell({ alertCount }) {
  const navigate = useNavigate();
  return (
    <button
      onClick={() => navigate('/alerts')}
      style={{
        background: alertCount > 0 ? 'rgba(255,34,85,.1)' : 'rgba(0,200,255,.05)',
        border: alertCount > 0 ? '1.5px solid rgba(255,34,85,.4)' : '1.5px solid var(--border)',
        color: alertCount > 0 ? 'var(--red)' : 'var(--dim)',
        cursor: 'pointer',
        padding: '7px 14px',
        display: 'flex', alignItems: 'center', gap: '8px',
        transition: 'all .2s',
        position: 'relative',
        boxShadow: alertCount > 0 ? '0 0 16px rgba(255,34,85,.3)' : 'none',
      }}
    >
      <span style={{
        fontSize: '18px',
        animation: alertCount > 0 ? 'pulse 1s infinite' : 'none',
        filter: alertCount > 0 ? 'drop-shadow(0 0 6px var(--red))' : 'none',
      }}>🔔</span>
      <span style={{
        fontFamily: "'Orbitron',sans-serif", fontSize: '10px', fontWeight: '700',
        letterSpacing: '.08em',
      }}>
        {alertCount > 0 ? `${alertCount} ALERT${alertCount > 1 ? 'S' : ''}` : 'NO ALERTS'}
      </span>
      {alertCount > 0 && (
        <span style={{
          position: 'absolute', top: '-6px', right: '-6px',
          width: '18px', height: '18px', borderRadius: '50%',
          background: 'var(--red)', color: '#fff',
          fontFamily: "'Orbitron',sans-serif", fontSize: '9px', fontWeight: '700',
          display: 'flex', alignItems: 'center', justifyContent: 'center',
          boxShadow: '0 0 10px rgba(255,34,85,.8)',
          animation: 'pulse 1s infinite',
        }}>{alertCount > 99 ? '99+' : alertCount}</span>
      )}
    </button>
  );
}

function ProtectedLayout() {
  const { user } = useAuth();
  const [liveEvents, setLiveEvents] = useState([]);
  const [popups, setPopups]         = useState([]);
  const [alertCount, setAlertCount] = useState(0);

  const handleWs = useCallback((msg) => {
    if (msg.type === 'device_event') {
      setLiveEvents(p => [...p.slice(-49), msg.data]);
    }
    if (msg.type === 'device_blocked') {
      // Show a special notification when agent physically blocks a device
      const id = Date.now();
      setPopups(p => [...p.slice(-3), {
        ...msg.data,
        _id: id,
        risk_level: 'critical',
        device_name: '🚫 DEVICE PHYSICALLY BLOCKED',
        hostname: msg.data.hostname,
      }]);
    }
    if (msg.type === 'device_alert') {
      const id = Date.now();
      setPopups(p => [...p.slice(-3), { ...msg.data, _id: id }]);
      setAlertCount(c => c + 1);
    }
  }, []);

  useWebSocket(handleWs);

  if (!user) return <Navigate to="/login" replace />;

  return (
    <div style={{ display: 'flex', minHeight: '100vh', background: 'var(--void)' }}>
      <Sidebar alertCount={alertCount} />

      <main style={{
        flex: 1,
        marginLeft: '240px',
        padding: '28px',
        minHeight: '100vh',
        background: 'var(--void)',
        position: 'relative',
      }}>
        {/* Grid background */}
        <div style={{
          position: 'fixed',
          top: 0, bottom: 0,
          left: '240px', right: 0,
          pointerEvents: 'none',
          zIndex: 0,
          backgroundImage: 'linear-gradient(rgba(0,200,255,0.025) 1px, transparent 1px), linear-gradient(90deg, rgba(0,200,255,0.025) 1px, transparent 1px)',
          backgroundSize: '50px 50px',
        }} />

        {/* ── Top Header Bar ── */}
        <div style={{
          position: 'sticky', top: 0, zIndex: 50,
          background: 'rgba(3,12,20,0.95)',
          borderBottom: '1px solid var(--border)',
          backdropFilter: 'blur(10px)',
          display: 'flex', alignItems: 'center', justifyContent: 'space-between',
          padding: '10px 0 10px 0',
          marginBottom: '24px',
          marginLeft: '-28px', marginRight: '-28px',
          paddingLeft: '28px', paddingRight: '28px',
        }}>
          {/* Left: breadcrumb */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <span style={{ fontFamily:"'Share Tech Mono',monospace", fontSize:'11px', color:'var(--dim)' }}>
              EDTMRS
            </span>
            <span style={{ color:'var(--border)', fontSize:'12px' }}>›</span>
            <span style={{ fontFamily:"'Orbitron',sans-serif", fontSize:'11px', color:'var(--blue)', letterSpacing:'.08em' }}>
              ADMIN CONSOLE
            </span>
          </div>

          {/* Right: alert bell + status */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '14px' }}>
            {/* Live indicator */}
            <div style={{ display: 'flex', alignItems: 'center', gap: '7px' }}>
              <span className="dot dot-on" />
              <span style={{ fontFamily:"'Share Tech Mono',monospace", fontSize:'10px', color:'var(--green)', letterSpacing:'.1em' }}>
                LIVE
              </span>
            </div>

            {/* Notification Bell */}
            <AlertBell alertCount={alertCount} />
          </div>
        </div>

        <div style={{ position: 'relative', zIndex: 1 }}>
          <Routes>
            <Route path="/"          element={<DashboardPage liveEvents={liveEvents} />} />
            <Route path="/devices"   element={<DevicesPage   liveEvents={liveEvents} />} />
            <Route path="/endpoints" element={<EndpointsPage liveEvents={liveEvents} />} />
            <Route path="/alerts"    element={<AlertsPage    liveEvents={liveEvents} onAlertCountChange={setAlertCount} />} />
            <Route path="/policy"    element={<PolicyPage />} />
            <Route path="*"          element={<Navigate to="/" replace />} />
          </Routes>
        </div>
      </main>

      {/* Alert popups - bottom right */}
      <div style={{
        position: 'fixed',
        bottom: '20px',
        right: '20px',
        zIndex: 300,
        display: 'flex',
        flexDirection: 'column',
        gap: '12px',
      }}>
        {popups.map(popup => (
          <AlertPopup
            key={popup._id}
            alert={popup}
            onClose={() => setPopups(p => p.filter(x => x._id !== popup._id))}
          />
        ))}
      </div>
    </div>
  );
}

function AppRoutes() {
  const { loading } = useAuth();
  if (loading) return (
    <div style={{
      minHeight: '100vh',
      background: 'var(--void)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
    }}>
      <div style={{
        width: '44px', height: '44px',
        border: '3px solid rgba(0,200,255,0.2)',
        borderTop: '3px solid var(--blue)',
        borderRadius: '50%',
        animation: 'spin 0.8s linear infinite',
      }} />
      <style>{`@keyframes spin { to { transform: rotate(360deg); } }`}</style>
    </div>
  );
  return (
    <Routes>
      <Route path="/login" element={<LoginPage />} />
      <Route path="/*"     element={<ProtectedLayout />} />
    </Routes>
  );
}

export default function App() {
  return (
    <AuthProvider>
      <BrowserRouter>
        <AppRoutes />
      </BrowserRouter>
    </AuthProvider>
  );
}
