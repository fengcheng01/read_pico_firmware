import React from 'react';
import ReactDOM from 'react-dom/client';
import { App } from './App';
import { FlasherSection } from './components/FlasherSection';
import './index.css';

const rootElement = document.getElementById('root');
if (rootElement) {
  ReactDOM.createRoot(rootElement).render(
    <React.StrictMode>
      {new URLSearchParams(window.location.search).get('tool') === 'flasher' ? (
        <main className="standalone-tool"><a className="text-link" href="./">← PageRest 首页</a><FlasherSection lang="zh" /></main>
      ) : <App />}
    </React.StrictMode>
  );
}
