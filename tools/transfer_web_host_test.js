/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 执行真实网页脚本，模拟网络与DOM验证用户流程。/ Run the actual webpage script with network and DOM boundaries mocked.
 */
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const source = fs.readFileSync('components/read_pico_transfer/upload.html', 'utf8').match(/<script>([\s\S]*?)<\/script>/)[1];
const flush = () => new Promise(resolve => setImmediate(resolve));

async function setup({existing = false, confirmations = [], responses = [], deleteResponse = {ok: true}, deleteStatus = 200, fontsEnabled = true, imagesEnabled = true, bookLimit = 0,
    fsListing = null, wifiConfigured = false, checkError = null, lastUpload = null, baseline = 12} = {}) {
  const all = [];
  class Element {
    constructor() { this.children = []; this.dataset = {}; this.style = {}; this.value = ''; this.hidden = false; this.disabled = false; all.push(this); }
    set textContent(text) { this.text = String(text); this.children = []; }
    get textContent() { return this.text || ''; }
    set innerHTML(_) { throw Error('Untrusted text must not use innerHTML'); }
    set className(v) { this._class = String(v); }
    get className() { return this._class || ''; }
    setAttribute(key, value) { (this.attrs ||= {})[key] = String(value); }
    getAttribute(key) { return this.attrs ? this.attrs[key] : undefined; }
    append(...children) { this.children.push(...children); }
    replaceChildren(...children) { this.children = children; }
  }
  const nodes = new Map();
  const document = {getElementById(id) { if (!nodes.has(id)) nodes.set(id, new Element()); return nodes.get(id); },
    createElement() { return new Element(); }, querySelectorAll() { return all.filter(e => e.dataset.mutate); }};
  const state = {sent: [], requested: [], deleted: 0, deletions: [], retried: 0, retryURLs: []};
  const policy = {mode: 'ap', root: '/sdcard/books', free_bytes: 100000000, file_limit: bookLimit,
    wifi_configured: wifiConfigured, wifi_ssid: wifiConfigured ? 'HomeNet' : '',
    fonts_enabled: fontsEnabled, font_root: '/sdcard/fonts', font_limit: 33554432,
    last_upload:lastUpload, images_enabled: imagesEnabled, image_root: '/sdcard/images', image_limit: 20 * 1024 * 1024};
  const fetch = async (url, options = {}) => {
    state.requested.push(url);
    let status = 200, body;
    if (url === '/info') body = policy;
    else if (url === '/fs' || url.startsWith('/fs?')) {
      if (!fsListing) { status = 404; body = {error: '目录不存在'}; }
      else body = fsListing(url);
    }
    else if (options.method === 'DELETE') { ++state.deleted; state.deletions.push(url); status = deleteStatus; body = deleteResponse; }
    else if(url.startsWith('/upload-check?')){status=checkError?507:200;body=checkError?{error:checkError}:{exists:existing,upload_seq:baseline};}
    else if (options.method === 'POST') { ++state.retried; state.retryURLs.push(url); body = {ok: true}; }
    else if (url.startsWith('/books?name=') || url.startsWith('/fonts?name=')) { status = existing ? 200 : 404; body = existing ? {item: {name: 'same.txt', size: 10}} : {error: '不存在'}; }
    else body = {root: '/sdcard/books', root_label: 'TF 卡', items: [], total: 0, pages: 0};
    return {ok: status === 200, status, json: async () => body};
  };
  class XHR {
    constructor() { this.upload = {}; }
    open(method, url) { this.url = url; assert.equal(method, 'PUT'); }
    send(file) {
      state.sent.push(this.url);
      this.upload.onprogress({loaded: file.size / 2, total: file.size});
      const response = responses.shift() || {status: 200, body: {ok: true}};
      if (response === 'pending') return;
      queueMicrotask(() => {
        if (response === 'error') return this.onerror();
        this.status = response.status; this.responseText = JSON.stringify(response.body); this.onload();
      });
    }
    abort() { this.onabort(); }
  }
  const context = vm.createContext({document, fetch, XMLHttpRequest: XHR, TextEncoder, TextDecoder, Date,
    confirm: () => confirmations.length ? confirmations.shift() : true});
  vm.runInContext(source, context); await flush(); await flush();
  const file = {name: '<book & test>.txt', size: 5000000};
  function item(name = file.name, kind = 'book') { return {file: {...file, name}, row: new Element(), kind}; }
  async function run(items) { context.testItems = items; return vm.runInContext('run(testItems)', context); }
  return {context, nodes, state, item, run};
}

(async () => {
  let t = await setup({existing: true, confirmations: [false]});
  let item = t.item(); await t.run([item]); assert.match(item.row.textContent, /跳过/); assert.equal(t.state.sent.length, 0);
  t = await setup({existing: true, confirmations: [true]}); item = t.item(); await t.run([item]);
  assert.match(t.state.sent[0], /overwrite=1/); assert.match(item.row.textContent, /已保存/);
  t = await setup({confirmations: [false], responses: [{status: 409, body: {conflict: true}}]});
  item = t.item(); await t.run([item]); assert.match(item.row.textContent, /跳过/); assert.equal(t.state.sent.length, 1);
  t = await setup({responses: ['error', {status: 200, body: {ok: true}}]});
  item = t.item(); await t.run([item]); assert.match(item.row.textContent, /网络中断/);
  await item.row.children.find(b => b.textContent === '重试此文件').onclick(); assert.match(item.row.textContent, /已保存/); assert.equal(t.state.sent.length, 2);
  t = await setup({responses: ['pending']}); const items = [t.item('first.txt'), t.item('second.txt')];
  const pending = t.run(items); await flush(); t.nodes.get('bookCancel').onclick(); await pending;
  assert.equal(t.state.sent.length, 1); assert.match(items[0].row.textContent, /取消/); assert.match(items[1].row.textContent, /未上传/);
  t = await setup({responses: [{status: 500, body: {committed: true, progress_cleanup_failed: true, error: '文件已保存，但旧阅读进度清理失败'}}]});
  item = t.item(); await t.run([item]); assert.match(item.row.textContent, /已保存/);
  await item.row.children.find(b => b.textContent === '重试清理阅读进度').onclick();
  assert.equal(t.state.sent.length, 1); assert.equal(t.state.retried, 1);
  t = await setup({responses: [{status: 500, body: {committed: true, progress_cleanup_failed: true, name: 'Actual.txt', error: '清理失败'}}]});
  item = t.item('actual.txt'); await t.run([item]);
  await item.row.children.find(b => b.textContent === '重试清理阅读进度').onclick();
  assert.match(t.state.retryURLs[0], /name=Actual\.txt&/);
  t = await setup({deleteResponse: {deleted: true, progress_cleanup_failed: true, name: 'Actual.txt', error: '清理失败'}});
  item = t.item('actual.txt'); t.context.testBook = item.file; t.context.testRow = item.row;
  await vm.runInContext('removeBook(testBook,testRow)', t.context);
  const notice = t.nodes.get('bookResults').children[0];
  await notice.children.find(b => b.textContent === '重试清理阅读进度').onclick();
  assert.match(t.state.retryURLs[0], /name=Actual\.txt&/);
  t = await setup({responses: [{status: 200, body: {ok: true, committed: true, storage_cleanup_failed: true, warning: '旧书备份已保留，请勿重复上传'}}]});
  item = t.item(); await t.run([item]); assert.match(item.row.textContent, /已保存.*备份已保留/);
  assert.equal(t.state.sent.length, 1); assert.equal(item.row.children.length, 0);
  t = await setup({confirmations: [false, true]}); item = t.item(); t.context.testBook = item.file; t.context.testRow = item.row;
  await vm.runInContext('removeBook(testBook,testRow)', t.context); assert.equal(t.state.deleted, 0);
  await vm.runInContext('removeBook(testBook,testRow)', t.context); assert.equal(t.state.deleted, 1);
  t = await setup({bookLimit: 1048576}); item = t.item('FullChinese.ttf', 'font');
  await t.run([item]); assert.match(t.state.sent[0], /^\/fonts\?name=FullChinese.ttf$/);
  assert(t.state.requested.some(u => u.startsWith('/upload-check?kind=font&name=FullChinese.ttf&size=')));
  assert.match(item.row.textContent, /已保存.*字体/); assert.equal(t.state.retried, 0);
  t = await setup({fontsEnabled: false}); item = t.item('Font.ttf', 'font'); await t.run([item]);
  assert.match(item.row.textContent, /TF 卡/); assert.equal(t.state.sent.length, 0); assert(t.nodes.get('sendFonts').disabled);
  t = await setup(); item = t.item('Font.otf', 'font'); await t.run([item]);
  assert.match(item.row.textContent, /仅支持 TTF/); assert.equal(t.state.sent.length, 0);
  item = t.item('Huge.ttf', 'font'); item.file.size = 33554433; await t.run([item]);
  assert.match(item.row.textContent, /超过单文件/); assert.equal(t.state.sent.length, 0);
  t = await setup({existing: true, confirmations: [false]}); item = t.item('Font.ttf', 'font'); await t.run([item]);
  assert.match(item.row.textContent, /已跳过/); assert.equal(t.state.sent.length, 0);
  t = await setup({existing: true, confirmations: [true]}); item = t.item('Font.ttf', 'font'); await t.run([item]);
  assert.match(t.state.sent[0], /^\/fonts\?.*overwrite=1/);
  t = await setup({responses: [{status: 422, body: {error: '字体校验失败'}}, {status: 200, body: {ok: true}}]});
  item = t.item('Font.ttf', 'font'); await t.run([item]); assert.match(item.row.textContent, /校验失败/);
  await item.row.children.find(b => b.textContent === '重试此文件').onclick();
  assert(t.state.sent.every(u => u.startsWith('/fonts?'))); assert.match(item.row.textContent, /已保存/);
  t = await setup({responses: ['pending']}); const fonts = [t.item('One.ttf', 'font'), t.item('Two.ttf', 'font')];
  const fontPending = t.run(fonts); await flush(); t.nodes.get('fontCancel').onclick(); await fontPending;
  assert.equal(t.state.sent.length, 1); assert.match(fonts[1].row.textContent, /未上传/);
  // 图片上传：走/images，预检写入/同名；竞争409仍显式确认，扩展名/上限/TF门槛保留。
  // Image uploads preflight writes/names; raced409 confirms replacement and existing bounds still apply.
  t = await setup(); item = t.item('山景.jpg', 'image'); item.file.size = 1000000; await t.run([item]);
  assert.match(t.state.sent[0], /^\/images\?name=/); assert.match(item.row.textContent, /已保存.*设为锁屏/);
  assert(t.state.requested.some(u => u.startsWith('/upload-check?kind=image&name=')));
  t = await setup({responses: [{status: 409, body: {error: '存在'}}, {status: 200, body: {ok: true}}], confirmations: [true]});
  item = t.item('山景.jpg', 'image'); item.file.size = 1000000; await t.run([item]);
  assert.match(t.state.sent[1], /^\/images\?.*overwrite=1/); assert.match(item.row.textContent, /已保存/);
  t = await setup({imagesEnabled: false}); item = t.item('Photo.png', 'image'); await t.run([item]);
  assert.match(item.row.textContent, /TF 卡/); assert.equal(t.state.sent.length, 0); assert(t.nodes.get('sendImages').disabled);
  t = await setup(); item = t.item('Icon.gif', 'image'); await t.run([item]);
  assert.match(item.row.textContent, /仅支持 JPG \/ PNG/); assert.equal(t.state.sent.length, 0);
  item = t.item('Huge.jpg', 'image'); item.file.size = 20 * 1024 * 1024 + 1; await t.run([item]);
  assert.match(item.row.textContent, /超过单文件/); assert.equal(t.state.sent.length, 0);
  // 暂存与派发：选择后按钮可用，派发进同一上传队列，完成后按钮回到禁用。
  // Staging and dispatch: selecting enables the button, dispatch feeds one queue, and the button reverts after completion.
  t = await setup(); assert.equal(t.nodes.get('send').disabled, true);
  await t.context.stage([{name: 'pick1.txt', size: 10}, {name: 'pick2.txt', size: 20}], 'book');
  assert.equal(t.nodes.get('send').disabled, false);
  await vm.runInContext('dispatch()', t.context); await flush(); await flush();
  assert.equal(t.state.sent.length, 2);
  assert.equal(t.nodes.get('send').disabled, true);
  // 选择前可见文件名、大小与类别，移除及忙碌禁用；手动选择与拖入共用实际入口。
  // Names, sizes and kinds are visible before sending; removal and busy locking share actual picker/drop handlers.
  t = await setup();
  t.nodes.get('files').onchange({target:{files:[{name:'<选中的书>.epub',size:2048}],value:'local'}});
  let selected=t.nodes.get('bookSelected').children;
  assert.equal(selected.length,1);assert.equal(selected[0].children[0].textContent,'<选中的书>.epub');
  assert.match(selected[0].children[1].textContent,/图书.*2 KB/);
  assert.match(t.nodes.get('bookSelectedSummary').textContent,/已选 1/);
  t.nodes.get('dropzone').ondrop({preventDefault(){},dataTransfer:{files:[{name:'封面.png',size:4096}]}});
  assert.equal(t.nodes.get('imageSelected').children.length,1);
  await selected[0].children.find(b=>b.textContent==='移除').onclick();
  assert.equal(t.nodes.get('bookSelected').children.length,0);assert(t.nodes.get('send').disabled);assert.match(t.nodes.get('imageSelectedSummary').textContent,/已选 1/);
  assert(!t.nodes.get('sendImages').disabled);
  vm.runInContext('lock(true)',t.context);
  assert(t.nodes.get('imageSelected').children[0].children.at(-1).disabled);
  vm.runInContext('lock(false)',t.context);
  await vm.runInContext('dispatch()',t.context);
  assert.equal(t.nodes.get('imageSelected').children.length,0);
  assert.match(t.state.sent[0],/^\/images/);
  t=await setup();t.context.stage([{name:'one.txt',size:2048},{name:'one.png',size:1024}],'book');
  await t.nodes.get('send').onclick();
  assert.equal(t.state.sent.length,1);assert.match(t.state.sent[0],/^\/upload/);
  assert.equal(t.nodes.get('imageSelected').children.length,1);assert(!t.nodes.get('sendImages').disabled);
  await t.nodes.get('sendImages').onclick();assert.equal(t.state.sent.length,2);assert.match(t.state.sent[1],/^\/images/);
  // 图书区自动分流：JPG 进图片队列启用图片按钮；同名重复拖入只保留一份；未知扩展名当场拒绝。
  // Book-zone auto routing: JPGs join the image queue and enable its button; duplicate drops
  // keep one copy; unknown extensions reject on the spot.
  t = await setup();
  await t.context.stage([{name: 'photo.jpg', size: 1000}], 'book');
  assert.equal(t.nodes.get('sendImages').disabled, false);
  assert.equal(t.nodes.get('send').disabled, true);
  await t.context.stage([{name: 'photo.jpg', size: 1000}], 'image');
  assert.match(t.nodes.get('imageProgressText').textContent, /已在队列中/);
  await vm.runInContext('dispatch()', t.context); await flush(); await flush();
  assert.deepEqual(t.state.sent, ['/images?name=photo.jpg']);
  t = await setup();
  await t.context.stage([{name: 'virus.exe', size: 10}], 'book');
  assert.match(t.nodes.get('bookProgressText').textContent, /仅支持 TXT \/ EPUB \/ JPG \/ PNG/);
  assert.equal(t.state.sent.length, 0);
  // 各类任务在本模块保留名称、进度、结果与取消状态。/ Each module retains names, progress, results and cancellation.
  for(const kind of ['book','image','font']) {
    t=await setup({responses:['pending']});
    const name={book:'本地.txt',image:'本地.png',font:'本地.ttf'}[kind];
    t.context.stage([{name,size:4096}],kind);
    const button={book:'send',image:'sendImages',font:'sendFonts'}[kind];
    const task=t.nodes.get(button).onclick();await flush();await flush();
    assert.match(t.nodes.get(kind+'SelectedSummary').textContent,/本次.*上传中/);
    assert.match(t.nodes.get(kind+'ProgressText').textContent,new RegExp(name));
    assert.equal(t.nodes.get(kind+'ProgressBar').style.width,'50%');
    assert.match(t.nodes.get(kind+'Results').children[0].textContent,new RegExp(name));
    assert(!t.nodes.get(kind+'Cancel').disabled);
    for(const other of ['book','image','font'].filter(x=>x!==kind)){
      assert(t.nodes.get(other+'Cancel').disabled);
      assert.equal(t.context.document.getElementById(other+'Results').children.length,0);
    }
    t.nodes.get(kind+'Cancel').onclick();await task;
    assert.match(t.nodes.get(kind+'SelectedSummary').textContent,/任务已结束/);
    assert.match(t.nodes.get(kind+'Results').children[0].textContent,/取消/);
  }
  // 写入检查失败不发送文件；断流只恢复本次序号/名称/大小匹配的设备错误。
  // Failed write probes send no body; recover only a matching current upload error after a dropped response.
  t=await setup({checkError:'创建临时文件：目录不可写（errno=30）'});item=t.item('fresh.png','image');await t.run([item]);
  assert.equal(t.state.sent.length,0);assert.match(item.row.textContent,/目录不可写/);
  t=await setup({existing:true,confirmations:[false]});item=t.item('same.png','image');await t.run([item]);assert.equal(t.state.sent.length,0);assert.match(item.row.textContent,/跳过/);
  t=await setup({existing:true,confirmations:[true]});item=t.item('same.png','image');await t.run([item]);assert.match(t.state.sent[0],/overwrite=1/);
  const last={seq:13,name:'fresh.png',total:5000000,status:507,received:4096,error:'写入TF卡：TF卡读写失败（errno=5）'};
  t=await setup({responses:['error'],lastUpload:last});item=t.item('fresh.png','image');await t.run([item]);assert.match(item.row.textContent,/写入TF卡.*设备收到 4 KB/);assert.equal(t.state.sent.length,1);
  t=await setup({responses:['error'],lastUpload:{...last,seq:12}});item=t.item('fresh.png','image');await t.run([item]);assert.match(item.row.textContent,/网络中断/);
  t=await setup({responses:['error'],lastUpload:{...last,name:'old.png'}});item=t.item('fresh.png','image');await t.run([item]);assert.match(item.row.textContent,/网络中断/);
  // 卡上内容三页签：字体/图片管理复用 /fs 列表（过滤文件夹、逐行可删）；区域禁用时页签隐藏。
  // Three content panes: font/image management reuse the /fs listing (folders filtered,
  // one delete per row); panes hide while their area is disabled.
  t = await setup({fsListing: url => ({root: '/sdcard/fonts', root_label: 'TF 卡', path: '', parent: '', truncated: false,
      entries: url.includes('root=fonts') ? [{name: '存档', dir: true, size: 0, txt: false}, {name: 'Noto.ttf', dir: false, size: 20000000, txt: false}] : []})});
  assert.equal(t.nodes.get('tabFonts').hidden, false);
  t.nodes.get('tabFonts').onclick(); await flush(); await flush();
  assert(t.state.requested.some(u => u === '/fs?root=fonts'));
  const fontRows = t.nodes.get('fontList').children;
  assert.equal(fontRows.length, 1, 'folder rows are filtered out');
  assert(fontRows[0].children.some(e => e.textContent === 'Noto.ttf'));
  assert.match(t.nodes.get('fontListState').textContent, /共 1 个字体/);
  await fontRows[0].children.find(b => b.textContent === '删除').onclick(); await flush(); await flush();
  assert.equal(t.state.deleted, 1);
  assert(t.state.deletions[0] === '/fonts?name=' + encodeURIComponent('Noto.ttf'));
  t.nodes.get('tabList').onclick();
  assert.equal(t.nodes.get('paneList').hidden, false);
  assert.equal(t.nodes.get('paneFonts').hidden, true);
  assert.equal(t.nodes.get('tabFonts').getAttribute('aria-selected'), 'false');

  // 图片管理：同一流程走 /images 删除；取消确认不发出请求。
  // Image management: the same flow deletes through /images; cancelling skips the request.
  t = await setup({fsListing: url => ({root: '/sdcard/images', root_label: 'TF 卡', path: '', parent: '', truncated: false,
      entries: url.includes('root=images') ? [{name: '山景.jpg', dir: false, size: 1000, txt: false}] : []})});
  t.nodes.get('tabImages').onclick(); await flush(); await flush();
  assert(t.state.requested.some(u => u === '/fs?root=images'));
  const imageRows = t.nodes.get('imageList').children;
  assert.equal(imageRows.length, 1);
  t = await setup({fsListing: url => ({root: '/sdcard/images', root_label: 'TF 卡', path: '', parent: '', truncated: false,
      entries: url.includes('root=images') ? [{name: '山景.jpg', dir: false, size: 1000, txt: false}] : []}),
    confirmations: [false]});
  t.nodes.get('tabImages').onclick(); await flush(); await flush();
  await t.nodes.get('imageList').children[0].children.find(b => b.textContent === '删除').onclick(); await flush();
  assert.equal(t.state.deleted, 0, 'cancelled confirmation deletes nothing');

  // 删除失败：错误留在本行，列表不重载。
  // Deletion failure: the error stays on the row and the list is not reloaded.
  t = await setup({fsListing: () => ({root: '/sdcard/fonts', root_label: 'TF 卡', path: '', parent: '', truncated: false,
      entries: [{name: 'Noto.ttf', dir: false, size: 20, txt: false}]}),
    deleteStatus: 409, deleteResponse: {error: '正在上传或切换网络，请稍后重试'}});
  t.nodes.get('tabFonts').onclick(); await flush(); await flush();
  await t.nodes.get('fontList').children[0].children.find(b => b.textContent === '删除').onclick(); await flush(); await flush();
  assert.equal(t.state.deleted, 1);
  assert(t.nodes.get('fontList').children[0].children.some(c => c.textContent.includes('正在上传或切换网络')));
  assert.equal(t.state.requested.filter(u => u === '/fs?root=fonts').length, 1);

  // 区域禁用：字体/图片页签隐藏，上传区保持原有门槛。
  // Disabled areas: font/image tabs hide while upload zones keep their gates.
  t = await setup({fontsEnabled: false, imagesEnabled: false});
  assert.equal(t.nodes.get('tabFonts').hidden, true);
  assert.equal(t.nodes.get('tabImages').hidden, true);

  // AP 配网表单：保存走 POST /wifi，遗忘走 DELETE /wifi，空 SSID 拒绝本地提交。
  // AP provisioning form: save posts /wifi, forget deletes /wifi, empty SSID rejects locally.
  t = await setup({wifiConfigured: true});
  t.context.document.getElementById('ssid').value = 'HomeNet';
  t.nodes.get('wifiForm').onsubmit({preventDefault() {}});
  await flush(); await flush();
  assert(t.state.requested.includes('/wifi'));
  assert.match(t.nodes.get('wifiResult').textContent, /已保存/);
  t.nodes.get('forgetWifi').onclick();
  await flush(); await flush();
  assert.equal(t.state.deletions.filter(u => u === '/wifi').length, 1);
  assert.match(t.nodes.get('wifiResult').textContent, /已遗忘/);
  t = await setup({wifiConfigured: true, confirmations: [false]});
  t.nodes.get('forgetWifi').onclick();
  await flush(); await flush();
  assert(!t.state.deletions.includes('/wifi'));
  t = await setup({});
  t.context.document.getElementById('ssid').value = '';
  t.nodes.get('wifiForm').onsubmit({preventDefault() {}});
  await flush(); await flush();
  assert(!t.state.requested.includes('/wifi'));
  assert.match(t.nodes.get('wifiResult').textContent, /请输入网络名称/);

  console.log('transfer web interaction tests passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
