let allow_beta_update = configs['allow_beta_update'];
let exitFlag = false;
let resp = new HTTPResponse("https://api.github.com/repos/qr243vbi/nekobox/releases");

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


var release_array = [];
var is_newer = false;
var stopFlag = false;
var note_pre_release = '';
var release_url = '';
var release_note = '';
var assets_name = '';
var release_download_url = '';

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
            if (asset_name.includes(search) && asset_name.endsWith(".zip")) {

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

                    release_array.push([release['tag_name'], release_note]);

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

let release_download_url_flag = (release_download_url == '');

log(translate("assets version is" + (is_newer ? "": " not") + " newer" + ((is_newer && release_download_url_flag) ? ", but download url is empty" : "") ), "Warn");

if (release_download_url_flag || !is_newer){
    warning("No update", "Update");
    is_newer = false;
}

}
