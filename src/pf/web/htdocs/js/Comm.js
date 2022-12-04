/* Comm.js
 * Fetch wrapper.
 * TODO WebSocket.
 */
 
export class Comm {
  static getDependencies() {
    return [Window];
  }
  constructor(window) {
    this.window = window;
  }
  
  fetchText(method, path, query, headers, body) {
    return this.fetch(method, path, query, headers, body, { returnType: "text" });
  }
  
  fetchBinary(method, path, query, headers, body) {
    return this.fetch(method, path, query, headers, body, { returnType: "arrayBuffer" });
  }
  
  fetchJson(method, path, query, headers, body) {
    return this.fetch(method, path, query, headers, body, { returnType: "json" });
  }
  
  fetch(method, path, query, headers, body, options) {
    const url = this._composeUrl(path, query);
    return this.window.fetch(url, this._composeFetchOptions(method, headers, body)).then((rsp) => {
      if (!rsp.ok) throw new Error(`${rsp.status} ${rsp.statusText}`);
      switch (options?.returnType || "text") {
        case "text": return rsp.text();
        case "arrayBuffer": return rsp.arrayBuffer();
        case "json": return rsp.json();
        default: return rsp;
      }
    });
  }
  
  _composeUrl(path, query) {
    if (!path) path = "/";
    if (!path.startsWith("/")) throw new Error(`Invalid fetch path '${path}', must start with '/'`);
    let url = this.window.location.protocol + "//" + this.window.location.host + path;
    if (query) url = `${url}?${this._composeQuery(query)}`;
    return url;
  }
  
  _composeQuery(query) {
    return Object.keys(query).map(k => encodeURIComponent(k) + '=' + encodeURIComponent(query[k])).join('&');
  }
  
  _composeFetchOptions(method, headers, body) {
    const options = { method };
    if (headers) options.headers = headers;
    if (body) options.body = body;
    return options;
  }
}

Comm.singleton = true;
