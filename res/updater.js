function _n(value) {
    return value == null ? "" : String(value);
}

function _b(value) {
    return ! (! (value));
}

function _a(value) {
    if (value == null) return [];
    if (Array.isArray(value)) return value.map(_n);
    return [_n(value)];
}

function _o(value) {
    if (value == null) return {};
    return Object.fromEntries(Object.entries(value))
}

function print(string){
    window.print(_n(string));
}

function ask(message, title, array){
    return window.ask(_n(message), _n(title), _a(array));
}

function curdir(){
    return window.tempdir();
}

function curdir_path(filename){
    return window.curdir() + '/' + _n(filename);
}

function open_url(url){
    return window.open_url(url);
}

window.tempdir_path = tempdir_path;

function download(url, fileName, ifexists){
    return window.download(_n(url), _n(fileName), _b(ifexists));
}

function warning(message, title){
    window.warning(_n(message), _n(title));
}

function info(message, title){
    window.info(_n(message), _n(title));
}

function log(message, title){
    window.log(_n(message), _n(title));
}

function translate(message){
    return window.translate(_n(message));
}

function get_jsdelivr_link(message){
    return window.get_jsdelivr_link(_n(message));
}

function file_exists(message){
    return window.file_exists(_n(message));
}

function get_locale(){
    return window.get_locale();
}

function httpget(url, message, title){
    if (message === undefined){
        message = 'Requesting update error: %1';
    }
    if (title === undefined){
        title = 'Updater';
    }
    let p = new HTTPResponse(url);
    let er = p.error;
    if(er) {
        warning(
            translate(_n(message)).
                replace('%1', er).
                replace('%2', url),
            translate(_n(title))
        );
    };
    return p;
}



function utf8Encode(str) {
  let bytes = [];
  let i = 0;

  while (i < str.length) {
    let codePoint = str.codePointAt(i);

    // Advance by 2 if surrogate pair, else 1
    i += codePoint > 0xFFFF ? 2 : 1;

    if (codePoint <= 0x7F) {
      // 1 byte
      bytes.push(codePoint);
    } else if (codePoint <= 0x7FF) {
      // 2 bytes
      bytes.push(
        0xC0 | (codePoint >> 6),
        0x80 | (codePoint & 0x3F)
      );
    } else if (codePoint <= 0xFFFF) {
      // 3 bytes
      bytes.push(
        0xE0 | (codePoint >> 12),
        0x80 | ((codePoint >> 6) & 0x3F),
        0x80 | (codePoint & 0x3F)
      );
    } else {
      // 4 bytes
      bytes.push(
        0xF0 | (codePoint >> 18),
        0x80 | ((codePoint >> 12) & 0x3F),
        0x80 | ((codePoint >> 6) & 0x3F),
        0x80 | (codePoint & 0x3F)
      );
    }
  }

  return bytes; // Array of byte values (0–255)
}

function customPadStart(str, targetLength, padString) {
  if (str.length >= targetLength) return str;
  padString = padString || ' ';
  let padLength = targetLength - str.length;
  let pad = '';
  while (pad.length < padLength) {
    pad += padString;
  }
  return pad.slice(0, padLength) + str;
}

function utf8EncodeHex(str) {
  let bytes = utf8Encode(_n(str));
  let hex = '';

  // Convert each byte to hex and concatenate
  for (let i = 0; i < bytes.length; i++) {
    hex += customPadStart(bytes[i].toString(16), 2, '0'); // Pad to 2 digits
  }

  return hex;
}

function stringToHex(str){
    return utf8EncodeHex(str);
}
