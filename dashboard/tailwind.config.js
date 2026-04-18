/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    "./public/index.html",
    "./src/**/*.{js,jsx,ts,tsx}"
  ],
  theme: {
    extend: {
      colors: {
        dark: {
          900: "#0a0e1a",
          800: "#0f1629",
          700: "#141d35",
          600: "#1a2540",
          500: "#1e2d4d",
        },
        accent: {
          blue: "#3b82f6",
          cyan: "#06b6d4",
          green: "#10b981",
          yellow: "#f59e0b",
          red: "#ef4444",
          purple: "#8b5cf6",
        }
      }
    }
  },
  plugins: []
}
