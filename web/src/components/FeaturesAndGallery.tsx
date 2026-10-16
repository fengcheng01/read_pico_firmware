import { useState } from 'react';
import { ArrowUpRight, BookOpen, CalendarDays, Library, Wifi } from 'lucide-react';
import { translations, type Lang } from '../utils/i18n';
import { DeviceMockup } from './DeviceMockup';
import home from '../assets/screenshots/home.png';
import shelf from '../assets/screenshots/shelf.png';
import reader from '../assets/screenshots/reader.png';
import journal from '../assets/screenshots/journal_today.png';
import quote from '../assets/screenshots/quote-lock.png';
import transfer from '../assets/screenshots/transfer.png';
import bluetooth from '../assets/screenshots/bluetooth.png';
const images = [home,shelf,reader,journal,quote,transfer,bluetooth];
const icons = [BookOpen,Library,CalendarDays,Wifi];
export const FeaturesAndGallery = ({lang}: {lang: Lang}) => {
  const t = translations[lang];
  const [active,setActive] = useState(1);
  const entry = t.gallery[active];
  return <>
    <section id="features" className="section container feature-section"><div className="section-heading"><p className="eyebrow">{t.featuresTag}</p><h2>{t.featuresTitle}</h2><p>{t.featuresIntro}</p></div><div className="feature-grid">{t.features.map((f,i)=>{const Icon=icons[i];return <article key={i} className="feature"><Icon size={24} strokeWidth={1.5} /><h3>{f.title}</h3><p>{f.desc}</p><span className="feature-label">{f.label}</span></article>;})}</div></section>
    <section id="gallery" className="gallery-section"><div className="container"><div className="section-heading gallery-heading"><div><p className="eyebrow">{t.galleryTag}</p><h2>{t.galleryTitle}</h2></div><p>{t.galleryIntro}</p></div><div className="gallery-layout"><div className="gallery-stage" role="tabpanel" id="screen-panel" aria-labelledby={`screen-tab-${active}`}><span className="gallery-number mono">0{active+1} / 07</span><DeviceMockup imageSrc={images[active]} altText={`${t.device} · ${entry.title}`} /><p className="screen-note">{t.screenNote}</p></div><div className="gallery-content"><div className="screen-tabs" role="tablist" aria-label={lang === 'zh' ? '固件界面' : 'Firmware screens'}>{t.gallery.map((item,i)=><button key={i} id={`screen-tab-${i}`} role="tab" aria-selected={active===i} aria-controls="screen-panel" tabIndex={active===i?0:-1} onClick={()=>setActive(i)} onKeyDown={e=>{let next=i;if(e.key==='ArrowDown'||e.key==='ArrowRight')next=(i+1)%images.length;else if(e.key==='ArrowUp'||e.key==='ArrowLeft')next=(i+images.length-1)%images.length;else if(e.key==='Home')next=0;else if(e.key==='End')next=images.length-1;else return;e.preventDefault();setActive(next);document.getElementById(`screen-tab-${next}`)?.focus();}}><span className="mono">0{i+1}</span><span>{item.title}</span><ArrowUpRight size={17}/></button>)}</div><div className="screen-description" aria-live="polite"><p>{entry.desc}</p><ul>{entry.points.map(point=><li key={point}>{point}</li>)}</ul></div></div></div></div></section>
  </>;
};
