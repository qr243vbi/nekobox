var route_profiles = [];
var route_profile_names = {};
let resp = new HTTPResponse("https://api.github.com/repos/qr243vbi/ruleset/git/trees/routeprofiles");
let release = JSON.parse(resp.text); // Assuming resp.data is a JSON string

let locale = get_locale();

let repl_reg = new RegExp('_', 'g');

release.tree.forEach(asset => {
    let profile = asset.path;
    if (profile.endsWith('.json') && (profile.toLowerCase().startsWith('bypass') || profile.toLowerCase().startsWith('proxy'))) {
        profile = profile.slice(0, -5); // Remove the last 5 characters (".json")
        route_profiles.push(profile);
        route_profile_names[profile] = profile.replace(repl_reg, ' '); 
    }
});


let resp2 = new HTTPResponse("https://raw.githubusercontent.com/qr243vbi/ruleset/refs/heads/main/"+locale+".txt");
if (resp2.error == ''){
    let lines = resp2.text.split('\n');
    lines.forEach(line => {
        let ll = line.split(':');
        route_profile_names[ll[0]] = ll[1];
    });
}

function route_profile_get_json(profile){
    let url = "https://raw.githubusercontent.com/qr243vbi/ruleset/routeprofiles/" + profile + ".json";
    resp = new HTTPResponse(get_jsdelivr_link(url));
    let text = resp.text;
    if (!(resp.error == '')) {
        warning( translate("Requesting profile error: %1").replace("%1", resp.error + "\n" + text), translate("Download Profiles"));
        return "";
    } else {
        info( translate("Requesting profile success: %1").replace("%1", route_profile_names[profile] || profile), translate("Download Profiles"));
    }
    return [text, url];
}

