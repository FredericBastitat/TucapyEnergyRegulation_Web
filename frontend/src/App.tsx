import React, { useState, useEffect, useRef } from 'react';
import {
  Zap,
  Battery,
  Activity,
  Cpu,
  Terminal,
  RefreshCcw,
  Info,
  Lock,
  Database
} from 'lucide-react';
import { motion, AnimatePresence } from 'framer-motion';
import { ref, onValue } from 'firebase/database';
import { signInAnonymously } from 'firebase/auth';
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
}

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
    relayIdx: 0
  });

  const consoleRef = useRef<HTMLPreElement>(null);

  // Auto-scroll console
  useEffect(() => {
    if (consoleRef.current) {
      consoleRef.current.scrollTop = consoleRef.current.scrollHeight;
    }
  }, [data.logs]);

  // Handle Authentication and Firebase connection
  useEffect(() => {
    let unsubscribeEnergy: () => void;

    // Securely sign in anonymously
    signInAnonymously(auth)
      .then(() => {
        setData(prev => ({ ...prev, authenticated: true }));

        const energyRef = ref(db, 'energy_data');

        // Subscribe to energy updates (includes console logs)
        unsubscribeEnergy = onValue(energyRef, (snapshot: any) => {
          if (snapshot.exists()) {
            const val = snapshot.val();
            setData(prev => ({
              ...prev,
              batteryPower: val.battery_P || 0,
              batteryCurrent: val.battery_I || 0,
              gridCurrent: val.grid_I || 0,
              soc: val.battery_soc || 0,
              statusMsg: val.status_msg || prev.statusMsg,
              version: val.version || prev.version,
              logs: val.console_log || prev.logs,
              relayIdx: val.relay_idx || 0,
              connected: true
            }));
          } else {
            setData(prev => ({
              ...prev,
              statusMsg: 'Čekám na data z procesoru...',
              connected: true
            }));
          }
        });
      })
      .catch((error) => {
        console.error("Auth error:", error);
        setData(prev => ({ ...prev, statusMsg: 'Chyba zabezpečení (Auth)', logs: `Auth Error: ${error.message}` }));
      });

    return () => {
      if (unsubscribeEnergy) unsubscribeEnergy();
    };
  }, []);

  return (
    <div className="app-container">
      <header>
        <motion.div
          initial={{ opacity: 0, x: -20 }}
          animate={{ opacity: 1, x: 0 }}
          className="header-left"
        >
          <h1>Tucapy Energy</h1>
          <div className="version-tag">{data.version}</div>
        </motion.div>

        <motion.div
          initial={{ opacity: 0, scale: 0.8 }}
          animate={{ opacity: 1, scale: 1 }}
          className="header-right"
          style={{ display: 'flex', gap: '8px' }}
        >
          {data.authenticated && (
            <div className="status-badge secure">
              <Lock size={12} />
              Secure
            </div>
          )}
          <div className={`status-badge ${data.connected ? '' : 'offline'}`}>
            <Database size={14} />
            {data.connected ? 'Database Connected' : 'Connecting...'}
          </div>
        </motion.div>
      </header>

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
            <div key={i} className={`relay-item ${i < data.relayIdx ? 'active' : ''}`}>
              <div className="relay-led" />
              <span className="relay-label">OUT{i + 1}</span>
            </div>
          ))}
        </div>
      </motion.div>

      <motion.div
        initial={{ opacity: 0, y: 20 }}
        animate={{ opacity: 1, y: 0 }}
        transition={{ delay: 0.4 }}
        className="status-message"
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', justifyContent: 'center' }}>
          <Info size={16} />
          {data.statusMsg}
        </div>
      </motion.div>

      <motion.div
        initial={{ opacity: 0, y: 30 }}
        animate={{ opacity: 1, y: 0 }}
        transition={{ delay: 0.5 }}
        className="console-card"
      >
        <div className="console-header">
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Terminal size={14} />
            <span>SYSTEM CONSOLE LOGS</span>
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
      </motion.div>

      <footer>
        &copy; 2024 Tucapy Energy Regulation &bull; Firebase Infrastructure
      </footer>
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

