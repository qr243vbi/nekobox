function _n(string){
    if (string === undefined){
        return "";
    } else {
        return "" + string;
    }
}

function print(string){
    window.print(_n(string));
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
