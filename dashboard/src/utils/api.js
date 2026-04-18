// EDTMRS - API Service
// Centralized axios instance with JWT auth

import axios from 'axios';

const API_BASE = process.env.REACT_APP_API_URL || 'http://localhost:8000';

const api = axios.create({
  baseURL: API_BASE,
  timeout: 10000,
});

// Attach JWT token to every request
api.interceptors.request.use((config) => {
  const token = localStorage.getItem('edtmrs_token');
  if (token) config.headers.Authorization = `Bearer ${token}`;
  return config;
});

// Redirect to login on 401
api.interceptors.response.use(
  (res) => res,
  (err) => {
    if (err.response?.status === 401) {
      localStorage.removeItem('edtmrs_token');
      localStorage.removeItem('edtmrs_user');
      window.location.href = '/login';
    }
    return Promise.reject(err);
  }
);

export const authAPI = {
  login: (username, password) => api.post('/api/auth/login', { username, password }),
  me: () => api.get('/api/auth/me'),
};

export const statsAPI = {
  get: () => api.get('/api/stats'),
};

export const devicesAPI = {
  list: (params) => api.get('/api/devices', { params }),
};

export const endpointsAPI = {
  list: () => api.get('/api/endpoints'),
  isolate: (endpoint_id, reason) => api.post('/api/isolate-endpoint', { endpoint_id, reason }),
  unisolate: (id) => api.post(`/api/unisolate-endpoint/${id}`),
};

export const alertsAPI = {
  list: (params) => api.get('/api/alerts', { params }),
  acknowledge: (alert_id) => api.post('/api/alerts/acknowledge', { alert_id }),
  acknowledgeAll: () => api.post('/api/alerts/acknowledge-all'),
};

export const actionsAPI = {
  blockDevice: (data) => api.post('/api/block-device', data),
  whitelistDevice: (data) => api.post('/api/whitelist-device', data),
  getWhitelist: () => api.get('/api/whitelist'),
  getBlocked: () => api.get('/api/blocked-devices'),
  removeWhitelist: (id) => api.delete(`/api/whitelist/${id}`),
  unblockDevice: (id) => api.delete(`/api/blocked-devices/${id}`),
};

export const WS_URL = API_BASE.replace(/^http/, 'ws') + '/ws/alerts';

export const exportAPI = {
  devices: () => api.get('/api/export/devices', { responseType: 'blob' }),
  alerts:  () => api.get('/api/export/alerts',  { responseType: 'blob' }),
};

// Helper: trigger file download in browser
export function downloadCSV(blob, filename) {
  const url  = window.URL.createObjectURL(new Blob([blob]));
  const link = document.createElement('a');
  link.href  = url;
  link.setAttribute('download', filename);
  document.body.appendChild(link);
  link.click();
  link.remove();
  window.URL.revokeObjectURL(url);
}

export default api;
