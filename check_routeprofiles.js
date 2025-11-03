var route_profiles = [];
let resp = new HTTPResponse("https://api.github.com/repos/qr243vbi/ruleset/git/trees/routeprofiles");
let release = JSON.parse(resp.data); // Assuming resp.data is a JSON string

release.tree.forEach(asset => {
    let profile = asset.path;
    if (profile.endsWith('.json') && (profile.toLowerCase().startsWith('bypass') || profile.toLowerCase().startsWith('proxy'))) {
        profile = profile.slice(0, -5); // Remove the last 5 characters (".json")
        route_profiles.push(profile);
    }
});

function route_profile_get_json(str){
    resp = new HTTPResponse(get_jsdelivr_link("https://raw.githubusercontent.com/qr243vbi/ruleset/routeprofiles/" + profile + ".json"));
    if (!(resp.error == '')) {
        warning( translate("Requesting profile error: %1").replace("%1", resp.error + "\n" + resp.data), translate("Download Profiles"));

        return "";
    }
    log(resp.data)
    return resp.data;
}
