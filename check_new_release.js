var allow_beta_update = configs['allow_beta_update'];
var exitFlag = false;
var simple_mode = true;
var resp = new HTTPResponse("https://api.github.com/repos/qr243vbi/nekobox/releases");
var archive_extension = ".zip"
var updater_args = [];
var release_array = [];
var is_newer = false;
var stopFlag = false;
var note_pre_release = '';
var release_url = '';
var release_note = '';
var assets_name = '';
var options = {};
var release_download_url = '';
var archive_name = '';
var latest_tag_name = '';
var chocolatey_package = (GlobalMap['chocolatey_package'] == 'true');



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
  let bytes = utf8Encode(str);
  let hex = '';

  // Convert each byte to hex and concatenate
  for (let i = 0; i < bytes.length; i++) {
    hex += customPadStart(bytes[i].toString(16), 2, '0'); // Pad to 2 digits
  }

  return hex;
}


if (file_exists(env["APPIMAGE"])){
  if (file_exists(APPLICATION_DIR_PATH + "/" + env["NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE"])){
    archive_extension = ".AppImage";
    if (search == "linux-amd64"){
      search = "x86_64-linux";
    } else if (search == "linux-arm64"){
      search = "aarch64-linux";
    } else if (search == "linux-arm32"){
      search = "armhf-linux";
    } else if (search == "linux-i386"){
      search = "i686-linux";
    } else {
      archive_extension = ".zip"
    }
  }
}

function isNewer(assetName) {
    let curver = NKR_VERSION.trim();
    if (!curver) {
        return false;
    }

    const spl = assetName.split('-');
    if (spl.length < 2) {
        return false;
    }

    let version = spl[1]; // Extract version: 1.2.3
    if (spl.length >= 3) {
        const spl_2 = spl[2];
        if (spl_2.includes("beta") || spl_2.includes("alpha") || spl_2.includes("rc")) {
            version += "." + spl_2;
        }
    }

    const parts = version.split('.'); // [1, 2, 3, beta, 13]
    const currentParts = curver.split('.').map(part => part.replace("-", "."));

    if (parts.length < 3 || currentParts.length < 3) {
        log("1. Version strings seem to be invalid: " + curver + " and " + version);
        return false;
    }

    const verNums = [];
    const currNums = [];

    // Add base version first
    verNums.push(parseInt(parts[0], 10), parseInt(parts[1], 10), parseInt(parts[2], 10));
    if (parts.length > 3) {
        if (parts[3] === "alpha") verNums.push(1);
        if (parts[3] === "beta") verNums.push(2);
        if (parts[3] === "rc") verNums.push(3);
        if (parts.length > 4) verNums.push(parseInt(parts[4], 10));
    }

    currNums.push(parseInt(currentParts[0], 10), parseInt(currentParts[1], 10), parseInt(currentParts[2], 10));
    if (currentParts.length > 3) {
        if (currentParts[3] === "alpha") currNums.push(1);
        if (currentParts[3] === "beta") currNums.push(2);
        if (currentParts[3] === "rc") currNums.push(3);
        if (currentParts.length > 4) currNums.push(parseInt(currentParts[4], 10));
    }

    if (verNums.length < 3 || currNums.length < 3) {
        log("2. Version strings seem to be invalid: " + curver + " and " + version);
        return false;
    }

    for (let i = 0; i < 3; i++) {
        if (verNums[i] > currNums[i]) return true;
        if (verNums[i] < currNums[i]) return false;
    }

    // Equal base version, check beta-ness
    if (verNums.length === 5 && currNums.length === 3) return false;
    if (verNums.length === 3 && currNums.length === 5) return true;

    if (verNums.length === 5 && currNums.length === 5) {
        for (let i = 3; i < 5; i++) {
            if (verNums[i] > currNums[i]) return true;
            if (verNums[i] < currNums[i]) return false;
        }
    } else {
        return false;
    }

    return false;
}



if (resp.error){
    warning(
        translate('Requesting update error: %1').replace('%1', resp.error),
        translate('Update'));
} else {
    let array = JSON.parse(resp.text);
	
    for (let release of array){
        if (!allow_beta_update) {
            if (release["prerelease"]) {
                continue;
            }
        }
		
        for (let asset of release["assets"]) {
            let asset_name = asset["name"];

            if (asset_name.includes(search) && asset_name.endsWith(archive_extension)) {
                
				if (exitFlag){
                    let tag_name = release['tag_name'];
                    if (isNewer(asset_name)) {
                        release_array.push([tag_name, release['body']]);
                    } else {
                        stopFlag = true;
                    }
                } else {
                    note_pre_release = release["prerelease"] ? " (Pre-release)" : "";
                    release_url = release["html_url"];
                    release_note = release["body"];
                    assets_name = asset_name;
					latest_tag_name = release['tag_name'];
                    release_array.push([latest_tag_name, release_note]);
                    release_download_url = asset["browser_download_url"];
                    exitFlag = true;
                    is_newer = isNewer(assets_name);
                    stopFlag = !is_newer;
                }
                break;
            }
        }
        if (exitFlag) {
            if (stopFlag){
                break;
            }
        }
    }
    let release_length = release_array.length;
    if (release_length > 1) {
        let ar = release_array[0];
        release_note = ar[0] + ': ' + ar[1];
        for (let i = 1 ; i < release_length ; i++ ) {
            release_note += "\n"
            ar = release_array[i];
            release_note += ar[0] + ': ' + ar[1];
        }
    }
archive_name = "downloads/" + assets_name;

let release_download_url_flag = (release_download_url == '');

log(translate("assets version is" + (is_newer ? "": " not") + " newer" + ((is_newer && release_download_url_flag) ? ", but download url is empty" : "") ), "Warn");

if (release_download_url_flag || !is_newer){
    warning(translate("No update"), translate("Update"));
	is_newer = false;
} else {
	is_newer = false;
    let array = [translate("Cancel"), translate("Open in browser")];
    if (UpdaterExists){
        array.push(translate("Update"));
    }
    let index = ask(
        translate("Update found: %1\nRelease note:\n%2").
            replace('%1', assets_name).replace('%2', release_note),
        translate("Update"),
            array
    );
    if (index == 1){
        open_url(release_url);
    }
	
    if (index == 2){
		is_newer = false;
        let errors = download(release_download_url, archive_name, true);
		if (chocolatey_package){
			let nupkg_errors = download(
				"https://github.com/qr243vbi/nekobox/releases/download/"+
				latest_tag_name+"/nekobox."+latest_tag_name+".nupkg", 
				"downloads/nekobox."+latest_tag_name+".nupkg", true);
			if (nupkg_errors == ''){
				options['chocolatey_source'] = curdir_path('downloads');
			}
		}
        if (errors == ''){
            let index2 = ask(
                translate("Update is ready, restart to install?"),
                translate("Update"),
                [translate("Yes"), translate("No")]
            );
			if (index2 == 0){
				if (Object.keys(options).length > 0){
					options['version'] = latest_tag_name;
					updater_args = ['-options', utf8EncodeHex(JSON.stringify(options))]
				}
				is_newer = true;
			}
        } else {
            warning(errors, translate("Failed to download update assets"));
        }
    }
}

}

