#!/bin/bash
echo '[general]' > oscrc
echo 'apiurl=https://api.opensuse.org' >> oscrc
echo '[https://api.opensuse.org]' >> oscrc
echo "user=${OBS_USER}" >> oscrc
echo "pass=${OBS_PASSWORD}" >> oscrc
echo 'credentials_mgr_class=osc.credentials.PlaintextConfigFileCredentialsManager' >> oscrc

install -Dm644 oscrc "$HOME"/.config/osc/oscrc
osc token --trigger "${OBS_HOOK_TOKEN}"
