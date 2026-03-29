if (first_start){
    let array = [translate("OK"), translate("Open homepage"), translate("Open support channel")];
    let index = ask(
        "NekoBox is a cross-platform network client built for managing and utilizing modern proxy protocols. It provides a flexible interface for configuring routing, profiles, and connection settings. Available in "+ languages.length +" languages, making NekoBox is accessible to users around the world.",
        NKR_SOFTWARE_NAME,
            array
    );

    if (index == 1){
        open_url("https://github.com/qr243vbi/nekobox");
    } else if (index == 2){
        open_url("https://matrix.to/#/#nekobox_ex:opensuse.org");
    }
}
