import React, { useState, useEffect, useRef } from 'react';
import {
  Zap,
  Battery,
  Activity,
  Cpu,
  LayoutDashboard,
  Terminal,
  RefreshCcw,
  TrendingUp,
  Settings,
  Database,
  User as UserIcon,
  LogOut,
  X
} from 'lucide-react';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  AreaChart,
  Area
} from 'recharts';
import { motion, AnimatePresence } from 'framer-motion';
import { ref, onValue, set } from 'firebase/database';
import {
  signInAnonymously,
  onAuthStateChanged,
  signInWithEmailAndPassword,
  signOut
} from 'firebase/auth';
import type { User } from 'firebase/auth';
import { db, auth } from './firebase';
import './App.css';

interface DashboardData {
  batteryPower: number;
  batteryCurrent: number;
  gridCurrent: number;
  soc: number;
  statusMsg: string;
  logs: string;
  version: string;
  connected: boolean;
  authenticated: boolean;
  relayIdx: number;
  lastUpdate: number;
}

interface HistoryPoint {
  time: string;
  soc: number;
  power: number;
  grid: number;
  current: number;
}

const OFFLINE_TIMEOUT_S = 60; // 1 minute

const App: React.FC = () => {
  const [data, setData] = useState<DashboardData>({
    batteryPower: 0,
    batteryCurrent: 0,
    gridCurrent: 0,
    soc: 0,
    statusMsg: 'Initializace...',
    logs: 'Connecting to Firebase...',
    version: '1.0.0-rev',
    connected: false,
    authenticated: false,
    relayIdx: 0,
    lastUpdate: 0
  });

  const [isAdmin, setIsAdmin] = useState(false);
  const [showLogin, setShowLogin] = useState(false);
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [authError, setAuthError] = useState('');
  const [activeTab, setActiveTab] = useState<'dashboard' | 'graphs' | 'console' | 'settings'>('dashboard');
  const [dataHistory, setDataHistory] = useState<HistoryPoint[]>([]);
  const [os, setOs] = useState<'windows' | 'linux' | 'other'>('other');

  useEffect(() => {
    const platform = window.navigator.platform.toLowerCase();
    if (platform.includes('win')) setOs('windows');
    else if (platform.includes('linux')) setOs('linux');
  }, []);

  // Settings state
  const [settings, setSettings] = useState({
    upperCurrent: 2,
    lowerCurrent: 0,
    upperSoc: 80,
    lowerSoc: 60
  });

  // Save status: 'idle' | 'saving' | 'saved' | 'error'
  const [saveStatus, setSaveStatus] = useState<'idle' | 'saving' | 'saved' | 'error'>('idle');

  // Force re-render every 30s to update "Online/Offline" status
  const [, setTick] = useState(0);
  useEffect(() => {
    const interval = setInterval(() => setTick(t => t + 1), 30000);
    return () => clearInterval(interval);
  }, []);

  const consoleRef = useRef<HTMLPreElement>(null);

  // Auto-scroll console
  useEffect(() => {
    if (consoleRef.current) {
      consoleRef.current.scrollTop = consoleRef.current.scrollHeight;
    }
  }, [data.logs]);

  // Check if system is online (last update < 1 minute)
  const isSystemOnline = data.lastUpdate > 0 && (Math.floor(Date.now() / 1000) - data.lastUpdate) < OFFLINE_TIMEOUT_S;

  // Handle Authentication and Firebase connection
  useEffect(() => {
    let unsubscribeBattery: (() => void) | undefined;
    let unsubscribeWeb: (() => void) | undefined;
    let unsubscribeRecovery: (() => void) | undefined;
    let unsubscribeConnected: (() => void) | undefined;

    const unsubscribeAuth = onAuthStateChanged(auth, (user: User | null) => {
      if (user) {
        setData(prev => ({ ...prev, authenticated: true }));
        setIsAdmin(!user.isAnonymous);

        // --- 0. NEZÁVISLÝ STAV SPOJENÍ S FIREBASE ---
        const connectedRef = ref(db, '.info/connected');
        unsubscribeConnected = onValue(connectedRef, (snap) => {
          setData(prev => ({ ...prev, connected: snap.val() === true }));
        });

        // --- 1. DATA O ELEKTŘINĚ ---
        const batteryRef = ref(db, 'battery_data');
        unsubscribeBattery = onValue(batteryRef, (snapshot) => {
          if (snapshot.exists()) {
            const val = snapshot.val();
            setData(prev => ({
              ...prev,
              batteryPower: val.P || 0,
              batteryCurrent: val.I || 0,
              gridCurrent: val.grid_I || 0,
              soc: val.soc || 0
            }));

            // Historie pro grafy
            const now = new Date();
            const timeStr = `${now.getHours()}:${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')}`;
            setDataHistory(prev => {
              const newPoint: HistoryPoint = {
                time: timeStr,
                soc: val.soc || 0,
                power: val.P || 0,
                grid: val.grid_I || 0,
                current: val.I || 0
              };
              const next = [...prev, newPoint];
              return next.length > 100 ? next.slice(next.length - 100) : next;
            });
          }
        });

        // --- 2. SYSTÉMOVÁ DATA (WEB & LOGY) ---
        const webDataRef = ref(db, 'web_data');
        unsubscribeWeb = onValue(webDataRef, (snapshot) => {
          if (snapshot.exists()) {
            const val = snapshot.val();
            setData(prev => ({
              ...prev,
              statusMsg: val.status_msg || prev.statusMsg,
              version: val.version || prev.version,
              logs: val.console_log || prev.logs
            }));
          }
        });

        // --- 3. RECOVERY & STAV ---
        const recoveryRef = ref(db, 'recovery_data');
        unsubscribeRecovery = onValue(recoveryRef, (snapshot) => {
          if (snapshot.exists()) {
            const val = snapshot.val();
            setData(prev => ({
              ...prev,
              relayIdx: val.idx || 0, // Opraveno na "idx"
              lastUpdate: val.last_update || 0
            }));
          }
        });

        // --- 4. KONFIGURACE (pouze pro admina) ---
        if (!user.isAnonymous) {
          const configRef = ref(db, 'config_data');
          onValue(configRef, (snapshot) => {
            if (snapshot.exists()) {
              const val = snapshot.val();
              setSettings({
                upperCurrent: val.upper_current ?? 2,
                lowerCurrent: val.lower_current ?? 0,
                upperSoc: val.upper_soc ?? 80,
                lowerSoc: val.lower_soc ?? 60
              });
            }
          }, { onlyOnce: true });
        }
      } else {
        // Fallback to anonymous for basic viewing
        setActiveTab('dashboard');
        signInAnonymously(auth).catch(err => {
          console.error("Anon login error:", err);
          setData(prev => ({ ...prev, statusMsg: 'Chyba připojení k serveru' }));
        });
      }
    });

    return () => {
      unsubscribeAuth();
      if (unsubscribeBattery) unsubscribeBattery();
      if (unsubscribeWeb) unsubscribeWeb();
      if (unsubscribeRecovery) unsubscribeRecovery();
      if (unsubscribeConnected) unsubscribeConnected();
    };
  }, []);

  const handleLogin = async (e: React.FormEvent) => {
    e.preventDefault();
    setAuthError('');
    try {
      await signInWithEmailAndPassword(auth, email, password);
      setShowLogin(false);
      setEmail('');
      setPassword('');
    } catch (err: any) {
      setAuthError('Neplatné jméno nebo heslo');
    }
  };

  const handleLogout = () => {
    signOut(auth);
  };

  const handleSaveSettings = async () => {
    setSaveStatus('saving');
    try {
      await set(ref(db, 'config_data'), {
        upper_current: settings.upperCurrent,
        lower_current: settings.lowerCurrent,
        upper_soc: settings.upperSoc,
        lower_soc: settings.lowerSoc
      });
      setSaveStatus('saved');
      setTimeout(() => setSaveStatus('idle'), 3000);
    } catch (err) {
      console.error('Chyba při ukládání konfigurace:', err);
      setSaveStatus('error');
      setTimeout(() => setSaveStatus('idle'), 4000);
    }
  };

  return (
    <div className={`app-wrapper has-sidebar os-${os}`}>
      <aside className="sidebar">
        <div className="sidebar-logo">
          <Zap size={24} color="#38bdf8" />
          <span>TUCAPY</span>
        </div>
        <nav className="sidebar-nav">
          <button
            className={`nav-item ${activeTab === 'dashboard' ? 'active' : ''}`}
            onClick={() => setActiveTab('dashboard')}
          >
            <LayoutDashboard size={20} />
            <span>Monitoring</span>
          </button>
          <button
            className={`nav-item ${activeTab === 'graphs' ? 'active' : ''}`}
            onClick={() => setActiveTab('graphs')}
          >
            <TrendingUp size={20} />
            <span>Analýza dat</span>
          </button>
          {isAdmin && (
            <>
              <button
                className={`nav-item ${activeTab === 'settings' ? 'active' : ''}`}
                onClick={() => setActiveTab('settings')}
              >
                <Settings size={20} />
                <span>Konfigurace</span>
              </button>
              <button
                className={`nav-item ${activeTab === 'console' ? 'active' : ''}`}
                onClick={() => setActiveTab('console')}
              >
                <Terminal size={20} />
                <span>Systémová konzole</span>
              </button>
            </>
          )}
        </nav>
        <div className="sidebar-footer">
          {isAdmin ? (
            <button className="logout-btn" onClick={handleLogout}>
              <LogOut size={18} />
              <span>Odhlásit</span>
            </button>
          ) : (
            <button className="nav-item login-nav" onClick={() => setShowLogin(true)}>
              <UserIcon size={20} />
              <span>Admin Login</span>
            </button>
          )}
        </div>
      </aside>

      <div className="main-content">
        <header>
          <motion.div
            initial={{ opacity: 0, x: -20 }}
            animate={{ opacity: 1, x: 0 }}
            className="header-left"
          >
            <div className="title-row">
              <h1>Tucapy Energy</h1>
              <div className="version-tag">{data.version}</div>
            </div>
            {isAdmin && (
              <div className="admin-subtitle">
                {activeTab === 'dashboard' ? 'Energetický přehled' :
                  activeTab === 'graphs' ? 'Analýza dat' :
                    activeTab === 'settings' ? 'Konfigurace' : 'Systémová konzole'}
              </div>
            )}
          </motion.div>

          <motion.div
            initial={{ opacity: 0, scale: 0.8 }}
            animate={{ opacity: 1, scale: 1 }}
            className="header-right"
            style={{ display: 'flex', gap: '8px' }}
          >
            {data.authenticated && (
              <div className={`status-badge ${isSystemOnline ? 'secure' : 'offline'}`}>
                <Cpu size={12} />
                {isSystemOnline ? 'ESP32 Online' : 'ESP32 Offline'}
              </div>
            )}
            <div className={`status-badge ${data.connected ? '' : 'offline'}`}>
              <Database size={14} />
              {data.connected ? 'Database Online' : 'Connecting...'}
            </div>
          </motion.div>
        </header>

        <AnimatePresence mode="wait">
          {activeTab === 'dashboard' ? (
            <motion.div
              key="dashboard"
              initial={{ opacity: 0, y: 10 }}
              animate={{ opacity: 1, y: 0 }}
              exit={{ opacity: 0, y: -10 }}
              className="view-container"
            >
              <div className="grid">
                <StatCard
                  icon={<Zap size={20} color="#38bdf8" />}
                  label="Battery Power"
                  value={data.batteryPower}
                  unit="kW"
                  index={0}
                />
                <StatCard
                  icon={<Activity size={20} color="#10b981" />}
                  label="Battery Current"
                  value={data.batteryCurrent}
                  unit="A"
                  index={1}
                />
                <StatCard
                  icon={<Cpu size={20} color="#fbbf24" />}
                  label="Grid Current"
                  value={data.gridCurrent}
                  unit="A"
                  index={2}
                />
                <StatCard
                  icon={<Battery size={20} color="#38bdf8" />}
                  label="Battery SOC"
                  value={data.soc}
                  unit="%"
                  showBar
                  index={3}
                />
              </div>

              <motion.div
                initial={{ opacity: 0, y: 20 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: 0.3 }}
                className="relays-container"
              >
                <div className="relays-header">
                  <Activity size={16} />
                  <span>ACTIVE OUTPUTS (RELAYS)</span>
                </div>
                <div className="relays-grid">
                  {[...Array(8)].map((_, i) => (
                    <div key={i} className={`relay-item ${isSystemOnline && i < data.relayIdx ? 'active' : ''}`}>
                      <div className="relay-led" />
                      <span className="relay-label">OUT{i + 1}</span>
                    </div>
                  ))}
                </div>
              </motion.div>


            </motion.div>
          ) : activeTab === 'graphs' ? (
            <motion.div
              key="graphs"
              initial={{ opacity: 0, scale: 0.98 }}
              animate={{ opacity: 1, scale: 1 }}
              exit={{ opacity: 0, scale: 0.98 }}
              className="view-container graphs-view"
            >
              <div className="charts-grid">
                <div className="chart-card">
                  <div className="chart-header">
                    <Battery size={16} />
                    <span>Battery State of Charge</span>
                  </div>
                  <div className="chart-body">
                    <ResponsiveContainer width="100%" height={250}>
                      <AreaChart data={dataHistory}>
                        <defs>
                          <linearGradient id="colorSoc" x1="0" y1="0" x2="0" y2="1">
                            <stop offset="5%" stopColor="#38bdf8" stopOpacity={0.3} />
                            <stop offset="95%" stopColor="#38bdf8" stopOpacity={0} />
                          </linearGradient>
                        </defs>
                        <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="#1e293b" />
                        <XAxis dataKey="time" hide />
                        <YAxis domain={[0, 100]} stroke="#475569" fontSize={12} unit="%" />
                        <Tooltip
                          contentStyle={{ background: '#0f172a', border: '1px solid #1e293b', borderRadius: '12px' }}
                          itemStyle={{ color: '#38bdf8' }}
                        />
                        <Area type="monotone" dataKey="soc" stroke="#38bdf8" fillOpacity={1} fill="url(#colorSoc)" strokeWidth={3} />
                      </AreaChart>
                    </ResponsiveContainer>
                  </div>
                </div>

                <div className="chart-card">
                  <div className="chart-header">
                    <Zap size={16} />
                    <span>Battery Power & Grid Flow</span>
                  </div>
                  <div className="chart-body">
                    <ResponsiveContainer width="100%" height={250}>
                      <LineChart data={dataHistory}>
                        <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="#1e293b" />
                        <XAxis dataKey="time" hide />
                        <YAxis stroke="#475569" fontSize={12} />
                        <Tooltip
                          contentStyle={{ background: '#0f172a', border: '1px solid #1e293b', borderRadius: '12px' }}
                        />
                        <Line type="monotone" dataKey="power" stroke="#fbbf24" strokeWidth={3} dot={false} name="Power (kW)" />
                        <Line type="monotone" dataKey="grid" stroke="#10b981" strokeWidth={2} dot={false} name="Grid (A)" />
                      </LineChart>
                    </ResponsiveContainer>
                  </div>
                </div>
              </div>

              <div style={{ marginTop: '20px' }}>
                <StatCard
                  icon={<TrendingUp size={20} color="#38bdf8" />}
                  label="Average SOC (current session)"
                  value={dataHistory.length > 0 ? Math.round(dataHistory.reduce((a, b) => a + b.soc, 0) / dataHistory.length) : 0}
                  unit="%"
                  index={0}
                />
              </div>
            </motion.div>
          ) : activeTab === 'settings' ? (
            <motion.div
              key="settings"
              initial={{ opacity: 0, x: 20 }}
              animate={{ opacity: 1, x: 0 }}
              exit={{ opacity: 0, x: -20 }}
              className="view-container"
            >
              <div className="settings-card">
                <div className="settings-header">
                  <Settings size={20} />
                  <span>Systémové Limity a Nastavení</span>
                </div>

                <div className="settings-grid">
                  <div className="settings-group">
                    <h4> Proudové limity (A)</h4>
                    <div className="input-row">
                      <div className="field">
                        <label>Horní proud (zapnutí)</label>
                        <input type="number" value={settings.upperCurrent} onChange={(e) => setSettings({ ...settings, upperCurrent: Number(e.target.value) })} />
                      </div>
                      <div className="field">
                        <label>Spodní proud (vypnutí)</label>
                        <input type="number" value={settings.lowerCurrent} onChange={(e) => setSettings({ ...settings, lowerCurrent: Number(e.target.value) })} />
                      </div>
                    </div>
                  </div>

                  <div className="settings-group">
                    <h4> Baterie SOC limity (%)</h4>
                    <div className="input-row">
                      <div className="field">
                        <label>Horní SOC (start aktivace)</label>
                        <input type="number" value={settings.upperSoc} onChange={(e) => setSettings({ ...settings, upperSoc: Number(e.target.value) })} />
                      </div>
                      <div className="field">
                        <label>Spodní SOC (totální vypnutí)</label>
                        <input type="number" value={settings.lowerSoc} onChange={(e) => setSettings({ ...settings, lowerSoc: Number(e.target.value) })} />
                      </div>
                    </div>
                  </div>
                </div>

                <div className="settings-footer">
                  {saveStatus === 'saved' && (
                    <p className="hint hint--success">✓ Konfigurace uložena do databáze.</p>
                  )}
                  {saveStatus === 'error' && (
                    <p className="hint hint--error">✗ Chyba při ukládání. Zkontrolujte připojení.</p>
                  )}
                  {saveStatus === 'idle' && (
                    <p className="hint">Změny se uloží do Firebase a ESP32 je načte automaticky.</p>
                  )}
                  <button
                    className={`save-btn ${saveStatus === 'saving' ? 'saving' : ''}`}
                    onClick={handleSaveSettings}
                    disabled={saveStatus === 'saving'}
                  >
                    {saveStatus === 'saving' ? 'Ukládání...' : 'Uložit konfiguraci'}
                  </button>
                </div>
              </div>
            </motion.div>
          ) : (
            <motion.div
              key="console"
              initial={{ opacity: 0, y: 10 }}
              animate={{ opacity: 1, y: 0 }}
              exit={{ opacity: 0, y: -10 }}
              className="view-container console-view"
            >
              <div className="console-card full-page">
                <div className="console-header">
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <Terminal size={14} />
                    <span>SYSTEM CONSOLE LOGS FROM ESP32</span>
                  </div>
                  <motion.div
                    animate={{ rotate: 360 }}
                    transition={{ duration: 4, repeat: Infinity, ease: "linear" }}
                  >
                    <RefreshCcw size={12} color="#94a3b8" />
                  </motion.div>
                </div>
                <pre ref={consoleRef} className="console-body">
                  {data.logs}
                </pre>
              </div>
            </motion.div>
          )}
        </AnimatePresence>
      </div>

      <AnimatePresence>
        {showLogin && (
          <motion.div
            className="modal-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
          >
            <motion.div
              className="login-modal"
              initial={{ scale: 0.9, y: 20 }}
              animate={{ scale: 1, y: 0 }}
              exit={{ scale: 0.9, y: 20 }}
            >
              <div className="modal-header">
                <h3>Admin Login</h3>
                <button className="close-btn" onClick={() => setShowLogin(false)}><X size={18} /></button>
              </div>
              <form onSubmit={handleLogin}>
                <div className="input-group">
                  <label>E-mail</label>
                  <input
                    type="email"
                    value={email}
                    onChange={(e) => setEmail(e.target.value)}
                    placeholder="admin@example.com"
                    required
                  />
                </div>
                <div className="input-group">
                  <label>Heslo</label>
                  <input
                    type="password"
                    value={password}
                    onChange={(e) => setPassword(e.target.value)}
                    placeholder="••••••••"
                    required
                  />
                </div>
                {authError && <div className="auth-error-msg">{authError}</div>}
                <button type="submit" className="login-submit-btn">Přihlásit se</button>
              </form>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
};

interface StatCardProps {
  icon: React.ReactNode;
  label: string;
  value: number;
  unit: string;
  showBar?: boolean;
  index: number;
}

const StatCard: React.FC<StatCardProps> = ({ icon, label, value, unit, showBar, index }) => {
  return (
    <motion.div
      initial={{ opacity: 0, y: 20 }}
      animate={{ opacity: 1, y: 0 }}
      transition={{ delay: 0.1 * index }}
      className="card"
    >
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
        <div className="card-title">{label}</div>
        {icon}
      </div>
      <div className="card-value-container">
        <AnimatePresence mode="wait">
          <motion.span
            key={value}
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -10 }}
            transition={{ duration: 0.2 }}
            className="card-value"
          >
            {value}
          </motion.span>
        </AnimatePresence>
        <span className="card-unit">{unit}</span>
      </div>
      {showBar && (
        <div className="soc-bar-container">
          <motion.div
            className="soc-bar"
            initial={{ width: 0 }}
            animate={{ width: `${value}%` }}
            transition={{ type: "spring", stiffness: 50 }}
            style={{
              backgroundColor: value < 20 ? '#ef4444' : (value < 50 ? '#fbbf24' : '#10b981')
            }}
          />
        </div>
      )}
    </motion.div>
  );
}

export default App;

